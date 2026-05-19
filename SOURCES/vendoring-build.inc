# maps the source file to the roles that use that file
# value can be string or space delimited list of strings
# role name `__collection` means - do not vendor into
# role, just vendor directly into the collection
declare -A plugin_map=(
  [ansible/posix/plugins/modules/selinux.py]=selinux
  [ansible/posix/plugins/modules/seboolean.py]=selinux
  [ansible/posix/plugins/modules/mount.py]=storage
  [ansible/posix/plugins/modules/rhel_facts.py]=__collection
  [ansible/posix/plugins/modules/rhel_rpm_ostree.py]=__collection
  [ansible/posix/plugins/module_utils/mount.py]=storage
  [ansible/posix/plugins/module_utils/_respawn.py]=selinux
  [community/general/plugins/modules/ini_file.py]="tlog ad_integration"
  [community/general/plugins/modules/modprobe.py]=ha_cluster
  [community/general/plugins/modules/redhat_subscription.py]=rhc
  [community/general/plugins/modules/rhsm_release.py]=rhc
  [community/general/plugins/modules/rhsm_repository.py]=rhc
  [community/general/plugins/modules/seport.py]=selinux
  [community/general/plugins/modules/sefcontext.py]=selinux
  [community/general/plugins/modules/selogin.py]=selinux
  [containers/podman/plugins/modules/podman_container_info.py]=podman
  [containers/podman/plugins/modules/podman_image.py]=podman
  [containers/podman/plugins/modules/podman_play.py]=podman
  [containers/podman/plugins/modules/podman_secret.py]=podman
  [containers/podman/plugins/module_utils/podman/common.py]=podman
  [containers/podman/plugins/module_utils/podman/podman_image_lib.py]=podman
  [containers/podman/plugins/module_utils/podman/quadlet.py]=podman
)

# fix the following issue
# ERROR: Found 1 pylint issue(s) which need to be resolved:
# ERROR: plugins/modules/rhsm_repository.py:263:8: wrong-collection-deprecated: Wrong collection name ('community.general') found in call to Display.deprecated or AnsibleModule.deprecate
sed "s/collection_name='community.general'/collection_name='%{collection_namespace}.%{collection_name}'/" \
  -i .external/community/general/plugins/modules/rhsm_repository.py

# fix ansible-test docstring issue in ini_file.py
# I think this only affects rhel9 ansible 2.14, but we don't need this at all
# ERROR: plugins/modules/ini_file.py:0:0: invalid-documentation: DOCUMENTATION.seealso.0.module: required key not provided @ data['seealso'][0]['module']. Got None
# ERROR: plugins/modules/ini_file.py:0:0: invalid-documentation: DOCUMENTATION.seealso.0.plugin: extra keys not allowed @ data['seealso'][0]['plugin']. Got 'ansible.builtin.ini'
# ERROR: plugins/modules/ini_file.py:0:0: invalid-documentation: DOCUMENTATION.seealso.0.plugin_type: extra keys not allowed @ data['seealso'][0]['plugin_type']. Got 'lookup'
sed '/^seealso/,/^author/{/^seealso/d;/^[ ]/d}' -i .external/community/general/plugins/modules/ini_file.py

fix_module_documentation() {
  local module_src doc_fragment_name df_dest_dir
  local -a paths
  module_src=".external/$1"
  sed ':a;N;$!ba;s/description:\n\( *\)/description:\n\1- "WARNING: Do not use this plugin directly! It is only for role internal use."\n\1/' \
    -i "$module_src"
  # grab documentation fragments
  for doc_fragment_name in $(awk -F'[ -]+' '/^extends_documentation_fragment:/ {reading = 1; next}; /^[ -]/ {if (reading) {print $2}; next}; /^[^ -]/ {if (reading) {exit}}' "$module_src"); do
    if [ "$doc_fragment_name" = files ]; then continue; fi  # this one is built-in
    df_dest_dir="%{collection_build_path}/plugins/doc_fragments"
    if [ ! -d "$df_dest_dir" ]; then
      mkdir -p "$df_dest_dir"
    fi
    paths=(${doc_fragment_name//./ })
    # if we ever have two different collections that have the same doc_fragment name
    # with different contents, we will be in trouble . . .
    # will have to make the doc fragment files unique, then edit $dest to use
    # the unique name
    doc_fragment_src=".external/${paths[0]}/${paths[1]}/plugins/doc_fragments/${paths[2]}.py"
    if [ -f "$doc_fragment_src" ]; then
      cp "$doc_fragment_src" "$df_dest_dir"
    elif [ "${paths[2]}" = files ]; then
      echo Skipping files.py - this is an Ansible built-in
    else
      echo ERROR: could not find "$doc_fragment_src"
    fi
  done
}

declare -a modules mod_utils collection_plugins
declare -A dests
# vendor in plugin files - fix documentation, fragments
for src in "${!plugin_map[@]}"; do
  roles="${plugin_map["$src"]}"
  if [ "$roles" = __collection ]; then
    collection_plugins+=("$src")
    case "$src" in
    */plugins/modules/*) fix_module_documentation "$src";;
    esac
  else
    case "$src" in
    */plugins/modules/*) srcdir=plugins/modules; subdir=library; modules+=("$src"); fix_module_documentation "$src";;
    */plugins/module_utils/*) srcdir=plugins/module_utils; mod_utils+=("$src") ;;
    */plugins/action/*) srcdir=plugins/action ;;
    esac
  fi
  for role in $roles; do
    if [ "$role" = __collection ]; then
      dest="%{collection_build_path}/plugins${src/#*plugins/}"
      dests["$dest"]=__collection
    else
      case "$src" in
      */plugins/module_utils/*) subdir="module_utils/${role}_lsr" ;;
      esac
      dest="$role/${src/#*${srcdir}/${subdir}}"
      dests["$dest"]="$role"
    fi
    destdir="$(dirname "$dest")"
    if [ ! -d "$destdir" ]; then
      mkdir -p "$destdir"
    fi
    cp -pL ".external/$src" "$dest"
  done
done

# remove the temporary .external directory after vendoring
rm -rf .external

# fix python imports to point from the old name to the new name
for dest in "${!dests[@]}"; do
  role="${dests["$dest"]}"
  for module in "${modules[@]}"; do
    python_name="$(dirname "$module")"
    python_name="${python_name////[.]}"
    sed -e "s/ansible_collections[.]${python_name}[.]/ansible.modules./" -i "$dest"
    # podman uses relative imports for module_utils
    if [ "$role" = podman ]; then
      sed -e "s/^from [.][.]module_utils[.]podman[.]/from ..module_utils.podman_lsr.podman./" -i "$dest"
    fi
  done
  for mod_util in "${mod_utils[@]}"; do
    # some mod_utils have subdirs, some do not
    split=(${mod_util//// })
    python_name="ansible_collections[.]${split[0]}[.]${split[1]}[.]plugins[.]module_utils[.]"
    sed -e "s/${python_name}/ansible.module_utils.${role}_lsr./" -i "$dest"
  done
  for plugin in "${collection_plugins[@]}"; do
    python_name="$(dirname "$plugin")"
    dest_python_name="%{collection_namespace}/%{collection_name}/plugins${python_name/#*plugins/}"
    src_python_name="ansible_collections.${python_name////[.]}"
    dest_python_name="ansible_collections.${dest_python_name////.}"
    sed -e "s/${src_python_name}/${dest_python_name}/" -i "$dest"
  done
done

# Replacing "linux-system-roles.rolename" with "rhel-system-roles.rolename" in each role
# Replacing "fedora.linux_system_roles." with "redhat.rhel_system_roles" in each role
# This is for the "roles calling other roles" case
# for podman, change the FQCN - using a non-FQCN module name doesn't seem to work,
# even for the legacy role format
for rolename in %{rolenames}; do
  find "$rolename" -type f -exec \
    sed -e "s/linux-system-roles[.]${rolename}\\>/%{roleinstprefix}${rolename}/g" \
        -e "s/fedora[.]linux_system_roles[.]/%{collection_namespace}.%{collection_name}./g" \
        -e "s/containers[.]podman[.]/%{collection_namespace}.%{collection_name}./g" \
        -e "s/community[.]general[.]/%{collection_namespace}.%{collection_name}./g" \
        -e "s/ansible[.]posix[.]/%{collection_namespace}.%{collection_name}./g" \
        -i {} \;
done

# add ansible-test ignores needed due to vendoring
# vendoring makes rhsm_repository.py lines too long :-(
for ansible_ver in 2.14 2.15 2.16; do
  ignore_file="podman/.sanity-ansible-ignore-${ansible_ver}.txt"
  cat >> "$ignore_file" <<EOF
plugins/module_utils/podman_lsr/podman/common.py compile-2.7!skip
plugins/module_utils/podman_lsr/podman/common.py import-2.7!skip
plugins/module_utils/podman_lsr/podman/podman_image_lib.py compile-2.7!skip
plugins/module_utils/podman_lsr/podman/podman_image_lib.py import-2.7!skip
plugins/module_utils/podman_lsr/podman/quadlet.py compile-2.7!skip
plugins/module_utils/podman_lsr/podman/quadlet.py import-2.7!skip
plugins/modules/manage_image_cache.py compile-2.7!skip
plugins/modules/manage_image_cache.py import-2.7!skip
plugins/modules/podman_image.py compile-2.7!skip
plugins/modules/podman_image.py import-2.7!skip
plugins/modules/podman_play.py import-2.7!skip
plugins/modules/podman_secret.py import-2.7!skip
plugins/modules/rhsm_repository.py pep8!skip
EOF
done
# these platforms still use python 3.5
for ansible_ver in 2.14 2.15; do
  ignore_file="podman/.sanity-ansible-ignore-${ansible_ver}.txt"
  cat >> "$ignore_file" <<EOF
plugins/module_utils/podman_lsr/podman/common.py compile-3.5!skip
plugins/module_utils/podman_lsr/podman/common.py import-3.5!skip
plugins/module_utils/podman_lsr/podman/podman_image_lib.py compile-3.5!skip
plugins/module_utils/podman_lsr/podman/podman_image_lib.py import-3.5!skip
plugins/module_utils/podman_lsr/podman/quadlet.py compile-3.5!skip
plugins/module_utils/podman_lsr/podman/quadlet.py import-3.5!skip
plugins/modules/manage_image_cache.py compile-3.5!skip
plugins/modules/manage_image_cache.py import-3.5!skip
plugins/modules/podman_image.py compile-3.5!skip
plugins/modules/podman_image.py import-3.5!skip
plugins/modules/podman_play.py import-3.5!skip
plugins/modules/podman_secret.py import-3.5!skip
EOF
done

# special handling for ansible.posix seboolean, selinux
# see https://github.com/ansible-collections/ansible.posix/pull/460#issuecomment-2479725049
for file in selinux/library/seboolean.py selinux/library/selinux.py; do
  sed -e '/^from ansible.module_utils.selinux_lsr._respawn import respawn_module, HAS_RESPAWN_UTIL/a\
HAS_RESPAWN_UTIL = False\
\
\
def respawn_module(module_name):\
    pass' \
  -e '/^from ansible.module_utils.selinux_lsr._respawn import respawn_module, HAS_RESPAWN_UTIL/d' \
  -i "$file"
done

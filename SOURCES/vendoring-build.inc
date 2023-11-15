# Unpack tar.gz to retrieve to be vendored modules and place them in the roles library.
# ansible.posix:
#   - library:
#     - Module selinux and seboolean for the selinux role
#     - Module mount for the storage role
declare -A module_map=( ["selinux.py"]="selinux" ["seboolean.py"]="selinux" ["mount.py"]="storage" )
for module in "${!module_map[@]}"; do
  role="${module_map[${module}]}"
  if [ ! -d $role/library ]; then
    mkdir $role/library
  fi
  cp -pL .external/ansible/posix/plugins/modules/$module $role/library/$module
  sed -i -e ':a;N;$!ba;s/description:\n\( *\)/description:\n\1- WARNING: Do not use this module directly! It is only for role internal use.\n\1/' -e "s/ansible_collections.ansible.posix.plugins.module_utils/ansible.module_utils.${role}_lsr/" $role/library/$module
done

# ansible.posix:
#   - module_utils:
#     - Module_util mount for the storage role
module_map=( ["mount.py"]="storage" )
for module in "${!module_map[@]}"; do
  role="${module_map[${module}]}"
  if [ ! -d $role/module_utils/${role}_lsr ]; then
    mkdir -p $role/module_utils/${role}_lsr
  fi
  cp -pL .external/ansible/posix/plugins/module_utils/$module $role/module_utils/${role}_lsr/$module
done

# community.general:
#   - library:
#     - Module seport, sefcontext and selogin for the selinux role rolename2
#     - Module ini_file for role tlog
#     - rhc modules
#     - ha_cluster uses modprobe
module_map=( ["seport.py"]="selinux" ["sefcontext.py"]="selinux" ["selogin.py"]="selinux" ["ini_file.py"]="tlog"
  ["redhat_subscription.py"]="rhc" ["rhsm_release.py"]="rhc" ["rhsm_repository.py"]="rhc"
  ["modprobe.py"]="ha_cluster" )
for module in "${!module_map[@]}"; do
  role="${module_map[${module}]}"
  if [ ! -d $role/library ]; then
    mkdir $role/library
  fi
  # version 5.x seems to be broken?
  moduledir=.external/community/general/plugins/modules
  if [ ! -f $moduledir/$module ]; then
    moduledir=.external/community/general/plugins/modules/system
  fi
  if [ ! -f $moduledir/$module ]; then
    moduledir=.external/community/general/plugins/modules/files
  fi
  cp -pL $moduledir/$module $role/library/$module
  ls -alrtF $role/library/$module
  sed -i -e ':a;N;$!ba;s/description:\n\( *\)/description:\n\1- WARNING: Do not use this module directly! It is only for role internal use.\n\1/' $role/library/$module

  # Remove doc_fragments
  sed -i '/^extends_documentation_fragment:/,/^[^ -]/{/^extends/d;/^[ -]/d}' $role/library/$module
done

# containers.podman:
#   - library:
#     - Module podman_container_info, podman_image and podman_play for the podman role
module_map=( ["podman_container_info.py"]="podman" ["podman_image.py"]="podman" ["podman_play.py"]="podman"
  ["podman_secret.py"]="podman" )
for module in "${!module_map[@]}"; do
  role="${module_map[${module}]}"
  if [ ! -d $role/library ]; then
    mkdir $role/library
  fi
  moduledir=.external/containers/podman/plugins/modules
  cp -pL $moduledir/$module $role/library/$module
  ls -alrtF $role/library/$module
  sed -i -e ':a;N;$!ba;s/description:\n\( *\)/description:\n\1- WARNING: Do not use this module directly! It is only for role internal use.\n\1/' \
    -e "s/ansible_collections.containers.podman.plugins.module_utils.podman/ansible.module_utils.${role}_lsr/" \
    $role/library/$module
done

# containers.podman:
#   - module_utils:
#     - Module_util common for the podman role
module_map=( ["common.py"]="podman" )
for module in "${!module_map[@]}"; do
  role="${module_map[${module}]}"
  if [ ! -d $role/module_utils/${role}_lsr ]; then
    mkdir -p $role/module_utils/${role}_lsr
  fi
  cp -pL .external/containers/podman/plugins/module_utils/podman/$module $role/module_utils/${role}_lsr/$module
done

# remove the temporary .external directory after vendoring
rm -rf .external

# Replacing "linux-system-roles.rolename" with "rhel-system-roles.rolename" in each role
# Replacing "fedora.linux_system_roles." with "redhat.rhel_system_roles" in each role
# This is for the "roles calling other roles" case
# for podman, change the FQCN - using a non-FQCN module name doesn't seem to work,
# even for the legacy role format
# replace community.general for rhc
for rolename in %{rolenames}; do
    find $rolename -type f -exec \
         sed -e "s/linux-system-roles[.]${rolename}\\>/%{roleinstprefix}${rolename}/g" \
             -e "s/fedora[.]linux_system_roles[.]/%{collection_namespace}.%{collection_name}./g" \
             -e "s/containers[.]podman[.]/%{collection_namespace}.%{collection_name}./g" \
             -e "s/community[.]general[.]/%{collection_namespace}.%{collection_name}./g" \
             -i {} \;
done

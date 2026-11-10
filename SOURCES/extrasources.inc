Source801: https://galaxy.ansible.com/download/ansible-posix-2.1.0.tar.gz
Source901: https://galaxy.ansible.com/download/community-general-11.4.9.tar.gz
Source902: https://galaxy.ansible.com/download/containers-podman-1.20.2.tar.gz

Provides: bundled(ansible-collection(ansible.posix)) = 2.1.0
# we are stuck at this major version for community-general because newer
# versions do not support older python versions that RHEL 7 uses
Provides: bundled(ansible-collection(community.general)) = 11.4.9
Provides: bundled(ansible-collection(containers.podman)) = 1.20.2

Source996: CHANGELOG.rst
Source998: collection_readme.sh

#!/bin/bash
set -eo pipefail
# Called without GitHub credentials. AOSP envsetup is not nounset-compatible.
source "$(dirname "$0")/config.sh"
cd /srv/aegis/work/aosp
source build/envsetup.sh
lunch "$AOSP_LUNCH"
# Conservative parallelism for the initial 64 GB host.
jobs=$(nproc)
(( jobs <= 16 )) || jobs=16
m -j"$jobs"
get_build_var PRODUCT_OUT > "$1/product-out.txt"

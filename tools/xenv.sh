# Gate-machine X environment (PITS 317: gate assets must be rebuildable
# from the repo). Machine-specific: DISPLAY = the local X server.
export DISPLAY=:0
# 11-20: the SDDM's Xorg takes its auth from /run/user/<uid>/xauth_*
# (not ~/.Xauthority — that file holds a stale host entry that fails
# with "Authorization required, but no authorization protocol
# specified"). Detect it (the per-boot name varies).
_xauth=$(ls /run/user/$(id -u)/xauth_* 2>/dev/null | head -1)
if [ -n "$_xauth" ]; then
  export XAUTHORITY="$_xauth"
  unset _xauth
fi

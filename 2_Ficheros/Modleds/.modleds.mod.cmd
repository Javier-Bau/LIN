savedcmd_modleds.mod := printf '%s\n'   modleds.o | awk '!x[$$0]++ { print("./"$$0) }' > modleds.mod

#
# Pre-build script: inject the firmware version from git into a macro.
#
# The version is derived from `git describe`, so the in-code version string
# always matches the git tag (e.g. tag v1.1.0-beta.1 -> "1.1.0-beta.1").
# For untagged commits it becomes "<tag>-<n>-g<sha>", and "-dirty" is appended
# when the working tree has uncommitted changes.
#
import subprocess

Import("env")


def get_firmware_version():
    try:
        version = (
            subprocess.check_output(
                ["git", "describe", "--tags", "--always", "--dirty"],
                stderr=subprocess.DEVNULL,
            )
            .decode()
            .strip()
        )
    except Exception:
        version = "unknown"
    # Drop the leading "v" so the string matches the displayed version.
    return version[1:] if version.startswith("v") else version


version = get_firmware_version()
print("Firmware version: %s" % version)

env.Append(CPPDEFINES=[("FIRMWARE_VERSION", env.StringifyMacro(version))])

#
# Custom target: merge bootloader + partitions + boot_app0 + app into a single
# flashable image at offset 0x0.
#
# Defining the target here is cheap; the merge only runs when explicitly
# invoked with `pio run -t mergebin`. The filesystem image (SPIFFS) is NOT
# merged in -- it is released separately as spiffs.bin.
#
from os.path import join

Import("env")

board = env.BoardConfig()
mcu = board.get("build.mcu", "esp32")
platform = env.PioPlatform()


def merge_bin_action(target, source, env):
    esptool = join(platform.get_package_dir("tool-esptoolpy"), "esptool.py")
    firmware = env.subst("$BUILD_DIR/${PROGNAME}.bin")
    merged = env.subst("$BUILD_DIR/${PROGNAME}-merged.bin")
    app_offset = env.subst("$ESP32_APP_OFFSET") or "0x10000"

    # FLASH_EXTRA_IMAGES holds [offset, path, offset, path, ...] for the
    # bootloader, partition table and boot_app0.
    flash_images = [env.subst(x) for x in env.Flatten(env.get("FLASH_EXTRA_IMAGES", []))]

    cmd = [
        env.subst("$PYTHONEXE"),
        esptool,
        "--chip",
        mcu,
        "merge_bin",
        "-o",
        merged,
        "--flash_mode",
        board.get("build.flash_mode", "dio"),
        "--flash_size",
        board.get("upload.flash_size", "4MB"),
        *flash_images,
        app_offset,
        firmware,
    ]
    print("Merging firmware images -> %s" % merged)
    env.Execute(" ".join('"%s"' % c if " " in c else c for c in cmd))


env.AddCustomTarget(
    name="mergebin",
    dependencies="$BUILD_DIR/${PROGNAME}.bin",
    actions=merge_bin_action,
    title="Merge Firmware",
    description="Merge bootloader, partitions and app into <prog>-merged.bin",
)

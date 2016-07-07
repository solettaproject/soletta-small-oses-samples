# Soletta samples for Small OSes

## Applications

Each application contains its sources inside a src/ sub-directory, a
Makefile.application that configures the application, and optional
'config.$OS' directories where other per-application configuration files can
be found. What these files are may depend on the target OS, such as Zephyr
having a prj.conf file to configure the OS itself.
Other configuration files can be:
 * sol.conf: Override Soletta Kconfig variables
 * sol-flow.json: For FBP based applications, configures Flow parameters

All these configuration files may be include the board name targetted too, for
example:
 * sol_quark_se_devboard.conf
 * sol-flow_samr21-xpro.json
 * prj_qemu_x86.conf

## Build instructions

Inside a sample directory:

    make -C ../BUILD <os> BOARD=board_name [other targets]

Example:

    make -C ../BUILD zephyr BOARD=quark_se_devboard flash
    make -C ../BUILD riot BOARD=samr21-xpro debug

Supported OSes for the time being:
 * zephyr - [Zephyr website](https://www.zephyrproject.org/)
 * riot - [RIOT website](http://www.riot-os.org/)

Building for RIOT requires the variable RIOTBASE set to the path where
the RIOT sources can be found, and copying the `libsoletta` directory under
RIOT to the pkg/ subdirectory in RIOT itself.

Building for Zephyr requires:
 * `ZEPHYR_BASE` pointing to the Zephyr sources
 * `SOLETTA_BASE_DIR` pointing to the Soletta sources
 * `ZEPHYR_GCC_VARIANT` set to the toolchain variant to use (usually, "zephyr")
 * `ZEPHYR_SDK_INSTALL_DIR` set to the path where Zephyr's SDK is installed

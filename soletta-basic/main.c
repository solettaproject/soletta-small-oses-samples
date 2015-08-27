#include <stdio.h>

#include <sol-gpio.h>
#include <sol-mainloop.h>
#include <sol-network.h>

#include <periph_cpu.h>

static void
button_pressed(void *data, struct sol_gpio *gpio)
{
    static bool toggle = false;

    toggle = !toggle;
    sol_gpio_write(data, toggle);
}

static struct sol_gpio *
setup_button(const struct sol_gpio *led)
{
    struct sol_gpio_config conf = {
        .api_version = SOL_GPIO_CONFIG_API_VERSION,
        .dir = SOL_GPIO_DIR_IN,
        .in = {
            .trigger_mode = SOL_GPIO_EDGE_FALLING,
            .cb = button_pressed,
            .user_data = led
        }
    };

    printf("BUTTON: %lx\n", GPIO(PA, 28));
    return sol_gpio_open(GPIO(PA, 28), &conf);
}

static struct sol_gpio *
setup_led(void)
{
    struct sol_gpio_config conf = {
        .api_version = SOL_GPIO_CONFIG_API_VERSION,
        .dir = SOL_GPIO_DIR_OUT,
        .active_low = true
    };

    printf("LED: %lx\n", GPIO(PA, 19));
    return sol_gpio_open(GPIO(PA, 19), &conf);
}

int main(void)
{
    struct sol_gpio *led, *btn;
    const struct sol_vector *links;

    sol_init();
    if (!sol_network_init()) {
        puts("Network init failed");
        return -1;
    }

    puts("Up and running");

    links = sol_network_get_available_links();
    if (links) {
        const struct sol_network_link *l;
        uint16_t i;

        SOL_VECTOR_FOREACH_IDX(links, l, i) {
            uint16_t j;
            const struct sol_network_link_addr *addr;

            printf("Link #%d\n", i);
            SOL_VECTOR_FOREACH_IDX(&l->addrs, addr, j) {
                char buf[100];
                const char *ret;
                ret = sol_network_addr_to_str(addr, buf, sizeof(buf));
                if (ret)
                    printf("\tAddr #%d: %s\n", j, ret);
            }
        }
    }

    led = setup_led();
    if (!led) {
        puts("LED setup failed");
        for(;;);
    }

    btn = setup_button(led);
    if (!btn) {
        puts("Button setup failed");
        for(;;);
    }

    sol_run();

    sol_shutdown();

    return 0;
}

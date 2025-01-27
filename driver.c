#include "main_function.h"  

#pragma GCC optimize ("O0")

MODULE_LICENSE("GPL");

static int __init hpv_init(void){
    return start();
}

static void __exit hpv_exit(void){
    return end();
}

module_init(hpv_init);
module_exit(hpv_exit);
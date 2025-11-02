cd ./autark-cache/tests
file ./test6
#file ./autark-cache/autark
#set environment CC=clang
#set environment IWNET_RUN_TESTS=1
#set args /home/adam/Projects/softmotions/iwnet
#set args --prefix /home/adam/Projects/softmotions/iwnet/install /home/adam/Projects/softmotions/iwnet/autark-cache/extern_iowow

set confirm off
set follow-fork-mode parent
set detach-on-fork on
set print elements 4096

handle SIGUSR1 pass nostop print

define lb
    set breakpoint pending on
    source ~/.breakpoints-autark
    set breakpoint pending auto
    echo breakpoints loaded\n
end

define sb
    save breakpoints ~/.breakpoints-autark
    echo breakpoints saved\n
end
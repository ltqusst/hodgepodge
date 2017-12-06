#!/bin/bash

ECHO_PREFIX_INFO="\033[1;32;40mINFO...\033[0;0m"
ECHO_PREFIX_ERROR="\033[1;31;40mError...\033[0;0m"

# Try command  for test command result.
function try_command {
    echo ===try_command========
    echo "parameteres:"
    echo \$1:$1
    echo \$2:$2
    echo ...
    echo \$\@:$@
    echo ===try_command========
    "$@"
    status=$?
    if [ $status -ne 0 ]; then
        echo -e $ECHO_PREFIX_ERROR "ERROR with \"$@\", Return status $status."
        exit $status
    fi
    return $status
}


try_command echo "every thing follow function is param"
try_command echo 1 2 3 | cat

#https://www.gnu.org/software/bash/manual/html_node/Filename-Expansion.html#Filename-Expansion
echo ">>>>>>>>> Filename-Expansion " t?.cpp
echo ">>>>>>>>> Filename-Expansion " t??.cpp
echo ">>>>>>>>> Filename-Expansion " t????.cpp
echo ">>>>>>>>> Filename-Expansion " tt*.cpp
echo ">>>>>>>>> Filename-Expansion " t[1-6].cpp   # any char from '1' to '6'
echo ">>>>>>>>> Filename-Expansion " t[12df]*.cpp # any char of '1' '2' 'd' 'f'
echo ">>>>>>>>> Filename-Expansion " t[!a-z]*.cpp # not including 
echo ">>>>>>>>> Filename-Expansion " t[^a-z]*.cpp # not including

#Tilde Expansion
echo ">>>>>>>>>1" ~/dir1 ~hddls/dir2 ~xxx/dir3 ~+/dir4 ~-/dir5

#brace expansion
echo ">>>>>>>>>2" a{d,c,b}e

#https://www.gnu.org/software/bash/manual/html_node/Shell-Parameter-Expansion.html#Shell-Parameter-Expansion
#parameter="parameter"
word="word"
string=01234567890abcdefgh
echo ">>>>>>>>>3A" ${parameter:-word} ${string:7} ${string:7:2} ${string: -7}
parameter='word1 word2 word3 word4 word5'
echo ">>>>>>>>>3B" ${parameter#a* }
echo ">>>>>>>>>3B" ${parameter#w*2}      # remove words to the left  of the first match
echo ">>>>>>>>>3B" ${parameter##w*[24] } # remove words to the left  of the last  match
echo ">>>>>>>>>3B" ${parameter%w*4}      # remove words to the right of the first match
echo ">>>>>>>>>3B" ${parameter%%w*}      # remove words to the right of the last  match
FILENAME=bash_hackers.txt
echo ">>>>>>>>>3C" ${FILENAME%.txt}
echo ">>>>>>>>>3C" ${FILENAME%.*}
echo ">>>>>>>>>3C" ${FILENAME##*.}
DIR=/home/dir1/${FILENAME}
echo ">>>>>>>>>3D" ${DIR%/*}
echo ">>>>>>>>>3D" ${DIR##*/}


CWD_DIR=`pwd`
OS_ID=`lsb_release -is`
OS_VER=`lsb_release -ds | awk '{print $2}'`
KERNEL_RELEASE=`uname -r`
KERNEL_VERSION=`uname -r|awk -F "-" '{print $1}'`
KERNEL_ARCH=`uname -m`
corenum=`grep -c "processor" /proc/cpuinfo`

echo -e $ECHO_PREFIX_INFO $CWD_DIR
echo -e $ECHO_PREFIX_INFO OS_ID=$OS_ID 
echo -e $ECHO_PREFIX_INFO OS_VER=$OS_VER
echo -e $ECHO_PREFIX_INFO KERNEL_RELEASE=$KERNEL_RELEASE
echo -e $ECHO_PREFIX_INFO KERNEL_VERSION=$KERNEL_VERSION
echo -e $ECHO_PREFIX_INFO KERNEL_ARCH=$KERNEL_ARCH
echo -e $ECHO_PREFIX_INFO corenum=$corenum

MEDIASDK_DIR=opt/intel/mediasdk
OPENCL_DIR=opt/intel/opencl
ETC_OPENCL_DIR=etc/OpenCL

if [ ! -d $MEDIASDK_DIR ] && [ ! -d usr/lib64 ]; then
    echo -e $ECHO_PREFIX_ERROR "Cannot find installation content directory!"
fi



for ko_module in `find . -name bashscript.sh`
do
    echo ${ko_module}
done


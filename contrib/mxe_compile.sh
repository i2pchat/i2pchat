#!/bin/bash
# +++++++++++++++++++++++++++++++ 
# VORON HOW-TO
# git clone https://github.com/mxe/mxe.git
# cd mxe
# make qtbase
# make qtmultimedia
# make qt5
# // maybe one make qt5 try, but i did all
# +++++++++++++++++++++++++++++++ 
export MXE="/home/user/mxe/"
export MXEBIN="$MXE/usr/bin/"
export MXEQMAKE=$MXE/usr/i686-w64-mingw32.static/qt5/bin/qmake
init_MXEBIN_path(){
        echo $#
	if [ $# \> 0 ];then
		 echo "INIT YOUR OWN MXEBINPATH $1"
		 export PATH=$PATH:$1
        else
		 echo "USE DEFAULT MXEBINPATH $MXEBIN" 
		 export PATH=$PATH:$MXEBIN
	fi;	
}

start_qmake(){
	echo "==QMAKE START=="
	$MXEQMAKE
	LASTCODE=$?
	if [ $LASTCODE == 0 ];then
		echo "Qmake return 0, all ok"
	else
		echo "Qmake return $LASTCODE"
	fi;
	echo "==QMAKE DONE=="
}

start_compile(){
	make $@
}
cd ..
init_MXEBIN_path
start_qmake
start_compile


rm -rf temporary
mkdir temporary
cp -rv ../src temporary
cp -rv template/* temporary/

rcc --no-compress -g cpp ../resources/resources.qrc | sed '/#ifdef _MSC_VER/,/#endif/d' | sed -n '/#ifdef/q;p' > temporary/resources.cpp

cd temporary
python3 $XOVI_REPO/util/xovigen.py -o xovi.cpp -H xovi.h appload.xovi
qmake6 .
make -j`nproc`
cp appload.so ../
cd ..

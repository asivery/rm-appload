cp -rv template/* .
rm -rf temporary
mkdir -p temporary

cp -rv ../src temporary
rcc --no-compress -g cpp ../resources/resources.qrc | sed '/#ifdef _MSC_VER/,/#endif/d' | sed -n '/#ifdef/q;p' > temporary/resources.cpp
cp main.cpp config.h temporary/src/

qmake6 .
make

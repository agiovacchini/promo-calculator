#LD_LIBRARY_PATH=/home/utente/ws-cpp/libs/boost_1_60_0/stage/lib
valgrind  --tool=memcheck  -v --error-limit=no --log-file=./valgrind.log --leak-check=full --leak-resolution=high --num-callers=50 --freelist-vol=100000000 --trace-children=yes --malloc-fill=ac  --free-fill=fe --dsymutil=yes --track-origins=yes ./bin/promo-calculator ../PromoCalculator.deploy/ PromoCalculatorLnx.ini

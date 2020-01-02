Note that secp256k1 is configured withou gmp support. Later, try to enable it:

    ./configure --with-bignum=no  --enable-module-recovery

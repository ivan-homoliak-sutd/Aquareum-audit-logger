
# Dependencies
     $ sudo apt-get install build-essential
     $ sudo apt-get install libltdl7 libtool

# Create Configure file
    ./autogen.sh


# Note that secp256k1 is configured without gmp support:

    ./configure --with-bignum=no  --enable-module-recovery

    
Later, try to enable it.

# Setup notes

-install the packages in ./bin directory

# Execution notes

Before running any SGX application, the SDK library must be accessible through envirnoment variables:

-after the execution of ./bin/sgx-driver-for-ubuntu18  it is enough to run: $source ./sgxsdk/envirnoment


# Common problems:

## during compilation of apps:
/usr/bin/ld: cannot find -lsgx_tstdcxx

 Solution: libsgx_tstdcxx.a is old and was replaced by libsgx_tcxx.a; update -l param to -lsgx_tcxx

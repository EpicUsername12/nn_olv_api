# nn_olv API decompilation

A 'not 1:1 but close enough' decompilation of `nn_olv.rpl`

- **NOTE**: Some of the returned error codes 'Level' might be wrong, but the description and success/status is always respected.

The actual decompilation are in the `olv_*` files!

`main.cpp` is the code i use to test the code works.

The function `nn::olv::Report::Print` is stubbed in the real executable, but i changed it to call `WHBLogPrintf`.

# What's implemented

- Initialization
  - Account
  - Parental Control
  - Zlib
  - Param pack
  - ~Search Home Region (discovery)~
    - The request is done, but the XML parsing isn't implemented.

Once **Initialization** is done, we can start to actually implement each exported function!

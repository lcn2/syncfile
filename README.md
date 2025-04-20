# syncfile

sync between two files

When the `src` file is detected, we copy into a temp filename (`dest`
file with an file extension added) and then rename it to the `dest` file.
This means that the to file will never contain a partial copy of the
`src` file.  The `dest` file will either have its original contents or
the contents of the from file ... nothing in between.

The number of checks for testing that `src` file exists, which defaults
to 1 check, can be any number of checks including an unlimited number
of checks.  The interval in seconds between checks can also be set.


# To install

```sh
make clobber all
sudo make install clobber
```


# Example

Wait until the file `inbound` exists, and when it does, cause `outbound` to have the same contents.

```sh
$ /usr/local/bin/syncfile -n 0 inbound outbound
```


# To use

```
/usr/local/bin/syncfile [-h] [-v] [-V] [-f] [-d] [-D] [-T] [-c] [-t secs] [-n cnt] [-s suffix] src dest

	-h	   print this message
	-v	   output progress messages to stdout
	-V	   print version string and exit

	-f	   fork into background

	-d	   delete dest when src file does not exist
	-D	   delete src when dest file does not exist
	-T	   create/truncate files if one file is missing (conflicts with -d and -D)

	-c	   copy dest to src if dest is newer or src is gone (def: don't)

	-t secs	   check interval (may be a float) (def: 60.0)
	-n cnt	   number of checks, 0 ==> infinite (def: 1)

	-s suffix  filename suffix when forming new files (def: .new)

	src	   src file
	dest	   destination file

Exit codes:
    0         all OK
    2         -h and help string printed or -V and version string printed
    3         command line error
 >= 10        internal error

syncfile version: 1.6.1 2025-03-24
```


# Reporting Security Issues

To report a security issue, please visit "[Reporting Security Issues](https://github.com/lcn2/syncfile/security/policy)".

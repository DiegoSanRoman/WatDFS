//
// Starter code for CS 454/654
// You SHOULD change this file
//

#include "rpc.h"
#include "debug.h"
INIT_LOG

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <cstring>
#include <cstdlib>
#include <fcntl.h>
#include <fuse.h> 
#include <cstdint>

// Global state server_persist_dir.
char *server_persist_dir = nullptr;

// Important: the server needs to handle multiple concurrent client requests.
// You have to be careful in handling global variables, especially for updating them.
// Hint: use locks before you update any global variable.

// We need to operate on the path relative to the server_persist_dir.
// This function returns a path that appends the given short path to the
// server_persist_dir. The character array is allocated on the heap, therefore
// it should be freed after use.
// Tip: update this function to return a unique_ptr for automatic memory management.
char *get_full_path(char *short_path) {
    int short_path_len = strlen(short_path);
    int dir_len = strlen(server_persist_dir);
    int full_len = dir_len + short_path_len + 2; // +2 for '/' and '\0'

    char *full_path = (char *)malloc(full_len);
    if (!full_path) {
        DLOG("Error: Memory allocation failed in get_full_path");
        return nullptr;
    }

    // First fill in the directory.
    strcpy(full_path, server_persist_dir);
    strcat(full_path, "/");
    strcat(full_path, short_path);
    DLOG("Full path: %s", full_path);

    return full_path;
}

// The server implementation of getattr.
int watdfs_getattr(int *argTypes, void **args) {
    // Get the arguments.
    // The first argument is the path relative to the mountpoint.
    char *short_path = (char *)args[0];
    // The second argument is the stat structure, which should be filled in
    // by this function.
    struct stat *statbuf = (struct stat *)args[1];
    // The third argument is the return code, which should be set be 0 or -errno.
    int *ret = (int *)args[2];

    // Get the local file name, so we call our helper function which appends
    // the server_persist_dir to the given path.
    char *full_path = get_full_path(short_path);

    // Initially we set the return code to be 0.
    *ret = 0;

    // TODO: Make the stat system call, which is the corresponding system call needed
    // to support getattr. You should use the statbuf as an argument to the stat system call.
    (void)statbuf;
    int sys_ret = stat(full_path, statbuf);

    // Let sys_ret be the return code from the stat system call.

    if (sys_ret < 0) {
        // If there is an error on the system call, then the return code should
        // be -errno.
        *ret = -errno;
    }

    // Clean up the full path, it was allocated on the heap.
    free(full_path);

    //DLOG("Returning code: %d", *ret);
    // The RPC call succeeded, so return 0.
    return 0;
}

// The server implementation of mknod.
int watdfs_mknod(int *argTypes, void **args) {
    // 1) path
    char *short_path = (char *)args[0];
    // 2) mode_t (the client likely sent this as an int)
    mode_t mode = *(mode_t *)args[1];
    // 3) dev_t (the client likely sent this as a long if the harness demands ARG_LONG)
    dev_t dev = *(dev_t *)args[2];
    // 4) retcode
    int *ret = (int *)args[3];

    // Build the absolute path.
    char *full_path = get_full_path(short_path);

    // Call the system mknod
    *ret = 0; 
    int sys_ret = mknod(full_path, mode, dev);
    if (sys_ret < 0) {
        *ret = -errno;
    }

    free(full_path);
    return 0;  // Means the RPC itself did not fail
}

// The server implementation of open.
int watdfs_open(int *argTypes, void **args) {
    char *short_path = (char *) args[0];
    struct fuse_file_info *fi = (struct fuse_file_info *) args[1];
    int *ret = (int *) args[2];

    if (!short_path || !fi || !ret) {
        DLOG("Error: NULL argument in watdfs_open");
        return -EINVAL;
    }

    char *full_path = get_full_path(short_path);
    if (!full_path) {
        *ret = -ENOMEM;
        return 0;
    }

    *ret = 0;

    DLOG("Opening file: %s with flags: %d", full_path, fi->flags);
    int sys_fd = ::open(full_path, fi->flags, 0666); // Ensure permissions are set
    if (sys_fd < 0) {
        *ret = -errno;
    } else {
        fi->fh = (uint64_t) sys_fd;
        DLOG("File opened successfully, file descriptor: %d", sys_fd);
    }

    free(full_path);
    return 0;
}

// The server implementation of write
int watdfs_write(int *argTypes, void **args) {
    // 1) Path
    char *short_path = (char *)args[0];
    // 2) Buffer
    char *buf = (char *)args[1];
    // 3) Size
    size_t size = *(size_t *)args[2];
    // 4) Offset
    off_t offset = *(off_t *)args[3];
    // 5) File info
    struct fuse_file_info *fi = (struct fuse_file_info *)args[4];
    // 6) Return code
    int *ret = (int *)args[5];

    if (!short_path || !buf || !fi || !ret) {
        DLOG("Error: NULL argument in watdfs_write");
        return -EINVAL;
    }

    char *full_path = get_full_path(short_path);
    if (!full_path) {
        *ret = -ENOMEM;
        return 0;
    }

    // Ensure we have a valid file descriptor
    int fd = (int)fi->fh;
    if (fd <= 0) {
        DLOG("Error: Invalid file descriptor in watdfs_write");
        *ret = -EBADF; // Bad file descriptor
        free(full_path);
        return 0;
    }

    DLOG("Writing %zu bytes to file: %s at offset: %ld", size, full_path, (long)offset);

    // Perform the actual write
    ssize_t bytes_written = pwrite(fd, buf, size, offset);
    if (bytes_written < 0) {
        *ret = -errno;
        DLOG("Error: Write failed with errno: %d", errno);
    } else {
        *ret = (int)bytes_written;
        DLOG("Successfully wrote %d bytes", *ret);
    }

    free(full_path);
    return 0;
}

// The server implementation of read
int watdfs_read(int *argTypes, void **args) {
    // 1) Path
    char *short_path = (char *)args[0];
    // 2) Buffer
    char *buf = (char *)args[1];
    // 3) Size
    size_t size = *(size_t *)args[2];
    // 4) Offset
    off_t offset = *(off_t *)args[3];
    // 5) File info
    struct fuse_file_info *fi = (struct fuse_file_info *)args[4];
    // 6) Return code
    int *ret = (int *)args[5];

    if (!short_path || !buf || !fi || !ret) {
        DLOG("Error: NULL argument in watdfs_read");
        return -EINVAL;
    }

    char *full_path = get_full_path(short_path);
    if (!full_path) {
        *ret = -ENOMEM;
        return 0;
    }

    int fd = (int)fi->fh;
    if (fd <= 0) {
        DLOG("Error: Invalid file descriptor in watdfs_read");
        *ret = -EBADF;
        free(full_path);
        return 0;
    }

    DLOG("Reading %zu bytes from file: %s at offset: %ld", size, full_path, (long)offset);

    // Perform the actual read
    ssize_t bytes_read = pread(fd, buf, size, offset);
    if (bytes_read < 0) {
        *ret = -errno;
        DLOG("Error: Read failed with errno: %d", errno);
    } else {
        *ret = (int)bytes_read;
        DLOG("Successfully read %d bytes", *ret);
    }

    free(full_path);
    return 0;
}

// The server implementation of truncate
int watdfs_truncate(int *argTypes, void **args) {
    // 1) Path
    char *short_path = (char *)args[0];
    // 2) New Size
    off_t newsize = *(off_t *)args[1];
    // 3) Return Code
    int *ret = (int *)args[2];

    if (!short_path || !ret) {
        DLOG("Error: NULL argument in watdfs_truncate");
        return -EINVAL;
    }

    char *full_path = get_full_path(short_path);
    if (!full_path) {
        *ret = -ENOMEM;
        return 0;
    }

    DLOG("Truncating file: %s to new size: %ld", full_path, (long)newsize);

    // Perform the actual truncation
    int sys_ret = truncate(full_path, newsize);
    if (sys_ret < 0) {
        *ret = -errno;
        DLOG("Error: Truncate failed with errno: %d", errno);
    } else {
        *ret = 0;
        DLOG("Successfully truncated file: %s to size: %ld", full_path, (long)newsize);
    }

    free(full_path);
    return 0;
}

// The server implementation of fsync
int watdfs_fsync(int *argTypes, void **args) {
    // 1) Path
    char *short_path = (char *)args[0];
    // 2) File descriptor (received as an int)
    int fd = *(int *)args[1];
    // 3) Return Code
    int *ret = (int *)args[2];

    if (!short_path || !ret) {
        DLOG("Error: NULL argument in watdfs_fsync");
        return -EINVAL;
    }

    char *full_path = get_full_path(short_path);
    if (!full_path) {
        *ret = -ENOMEM;
        return 0;
    }

    DLOG("Syncing file: %s, fd: %d", full_path, fd);

    // Ensure the file descriptor is valid
    if (fd <= 0) {
        DLOG("Error: Invalid file descriptor in watdfs_fsync");
        *ret = -EBADF;
        free(full_path);
        return 0;
    }

    // Perform the actual fsync call
    int sys_ret = fsync(fd);
    if (sys_ret < 0) {
        *ret = -errno;
        DLOG("Error: fsync failed with errno: %d", errno);
    } else {
        *ret = 0;
        DLOG("Successfully synced file: %s", full_path);
    }

    free(full_path);
    return 0;
}


// The server implementation of ultimensat
int watdfs_utimensat(int *argTypes, void **args) {
    // 1) Path
    char *short_path = (char *)args[0];
    // 2) Timestamps (received as a raw char array)
    char *ts_raw = (char *)args[1];
    // 3) Number of timespec elements (should be 2)
    int num_ts = *(int *)args[2];
    // 4) Return Code
    int *ret = (int *)args[3];

    if (!short_path || !ts_raw || num_ts != 2 || !ret) {
        DLOG("Error: Invalid arguments in watdfs_utimensat");
        return -EINVAL;
    }

    char *full_path = get_full_path(short_path);
    if (!full_path) {
        *ret = -ENOMEM;
        return 0;
    }

    // Convert received raw char array into timespec array
    struct timespec ts[2];
    memcpy(&ts[0], ts_raw, sizeof(struct timespec));
    memcpy(&ts[1], ts_raw + sizeof(struct timespec), sizeof(struct timespec));

    DLOG("Updating timestamps for file: %s - atime: %ld sec, %ld nsec | mtime: %ld sec, %ld nsec",
         full_path, ts[0].tv_sec, ts[0].tv_nsec, ts[1].tv_sec, ts[1].tv_nsec);

    // Perform the actual utimensat call
    int sys_ret = utimensat(AT_FDCWD, full_path, ts, 0);
    if (sys_ret < 0) {
        *ret = -errno;
        DLOG("Error: utimensat failed with errno: %d", errno);
    } else {
        *ret = 0;
        DLOG("Successfully updated timestamps for: %s", full_path);
    }

    free(full_path);
    return 0;
}



// The main function of the server.
int main(int argc, char *argv[]) {
    // argv[1] should contain the directory where you should store data on the
    // server. If it is not present it is an error, that we cannot recover from.
    if (argc != 2) {
        // In general, you shouldn't print to stderr or stdout, but it may be
        // helpful here for debugging. Important: Make sure you turn off logging
        // prior to submission!
        // See watdfs_client.cpp for more details
        // # ifdef PRINT_ERR
        // std::cerr << "Usage:" << argv[0] << " server_persist_dir";
        // #endif
        return -1;
    }
    server_persist_dir = argv[1];
    // Store the directory in a global variable.
    server_persist_dir = argv[1];

    // TODO: Initialize the rpc library by calling `rpcServerInit`.
    // Important: `rpcServerInit` prints the 'export SERVER_ADDRESS' and
    // 'export SERVER_PORT' lines. Make sure you *do not* print anything
    // to *stdout* before calling `rpcServerInit`.
    //DLOG("Initializing server...");

    int ret = 0;
    int init_ret = rpcServerInit();
    // TODO: If there is an error with `rpcServerInit`, it maybe useful to have
    // debug-printing here, and then you should return.
    if (init_ret < 0) {
        // # ifdef PRINT_ERR
        // std::cerr << "Failed to initialize RPC server" << std::endl;
        // #endif
        return init_ret;
    }
    // TODO: Register your functions with the RPC library.
    // Note: The braces are used to limit the scope of `argTypes`, so that you can
    // reuse the variable for multiple registrations. Another way could be to
    // remove the braces and use `argTypes0`, `argTypes1`, etc.
    {   // Register the getattr function.
        // There are 3 args for the function (see watdfs_client.cpp for more
        // detail).
        int argTypes[4];
        // First is the path.
        argTypes[0] =
            (1u << ARG_INPUT) | (1u << ARG_ARRAY) | (ARG_CHAR << 16u) | 1u;
        // The second argument is the statbuf.
        argTypes[1] =
            (1u << ARG_OUTPUT) | (1u << ARG_ARRAY) | (ARG_CHAR << 16u) | 1u;
        // The third argument is the retcode.
        argTypes[2] = (1u << ARG_OUTPUT) | (ARG_INT << 16u);
        // Finally we fill in the null terminator.
        argTypes[3] = 0;

        // We need to register the function with the types and the name.
        ret = rpcRegister((char *)"getattr", argTypes, watdfs_getattr);
        if (ret < 0) {
            // It may be useful to have debug-printing here.
            return ret;
        }
    }

    {   // Register the mknod function.
        // The client calls "mknod" with 4 arguments: path, mode, dev, retcode
        int argTypes[5];
        // 1) path (input char array)
        argTypes[0] = (1u << ARG_INPUT) | (1u << ARG_ARRAY) 
                      | (ARG_CHAR << 16u) | 1u;
        // 2) mode (input int or long, depending on your client)
        argTypes[1] = (1u << ARG_INPUT) | (ARG_INT << 16u);
        // 3) dev (input long if your client used ARG_LONG for dev_t)
        argTypes[2] = (1u << ARG_INPUT) | (ARG_LONG << 16u);
        // 4) retcode (output int)
        argTypes[3] = (1u << ARG_OUTPUT) | (ARG_INT << 16u);
        argTypes[4] = 0;

        ret = rpcRegister((char *)"mknod", argTypes, watdfs_mknod);
        if (ret < 0) {
            return ret;
        }
    }

    {   // Register the open function.
        // The client calls "open" with 3 arguments: path, fuse_file_info, retcode
        int argTypes[4];
        // 1) path (input char array)
        argTypes[0] = (1u << ARG_INPUT) | (1u << ARG_ARRAY)
                      | (ARG_CHAR << 16u) | 1u;
        // 2) fi (in/out char array) sized to struct fuse_file_info
        argTypes[1] = (1u << ARG_INPUT) | (1u << ARG_OUTPUT) | (1u << ARG_ARRAY) | (ARG_CHAR << 16u) | (unsigned)sizeof(struct fuse_file_info);
        // 3) retcode (output int)
        argTypes[2] = (1u << ARG_OUTPUT) | (ARG_INT << 16u);
        argTypes[3] = 0;

        ret = rpcRegister((char *)"open", argTypes, watdfs_open);
        if (ret < 0) {
            return ret;
        }
    }

    {   // Register the write function.
        int argTypes[7];
        // 1) path (input char array)
        argTypes[0] = (1u << ARG_INPUT) | (1u << ARG_ARRAY)
                    | (ARG_CHAR << 16u) | 1u;
        // 2) buffer (input char array)
        argTypes[1] = (1u << ARG_INPUT) | (1u << ARG_ARRAY)
                    | (ARG_CHAR << 16u) | (unsigned)MAX_ARRAY_LEN;
        // 3) size (input long)
        argTypes[2] = (1u << ARG_INPUT) | (ARG_LONG << 16u);
        // 4) offset (input long)
        argTypes[3] = (1u << ARG_INPUT) | (ARG_LONG << 16u);
        // 5) file info (input char array)
        argTypes[4] = (1u << ARG_INPUT) | (1u << ARG_ARRAY)
                    | (ARG_CHAR << 16u) | (unsigned)sizeof(struct fuse_file_info);
        // 6) return code (output int)
        argTypes[5] = (1u << ARG_OUTPUT) | (ARG_INT << 16u);
        argTypes[6] = 0; // Null terminator

        ret = rpcRegister((char*)"write", argTypes, watdfs_write);
        if (ret < 0) {
            return ret;
        }
    }

    {   // Register the read function. 
        int argTypes[7];
        // 1) path (input char array)
        argTypes[0] = (1u << ARG_INPUT) | (1u << ARG_ARRAY)
                    | (ARG_CHAR << 16u) | 1u;
        // 2) buffer (output char array)
        argTypes[1] = (1u << ARG_OUTPUT) | (1u << ARG_ARRAY)
                    | (ARG_CHAR << 16u) | (unsigned)MAX_ARRAY_LEN;
        // 3) size (input long)
        argTypes[2] = (1u << ARG_INPUT) | (ARG_LONG << 16u);
        // 4) offset (input long)
        argTypes[3] = (1u << ARG_INPUT) | (ARG_LONG << 16u);
        // 5) file info (input char array)
        argTypes[4] = (1u << ARG_INPUT) | (1u << ARG_ARRAY)
                    | (ARG_CHAR << 16u) | (unsigned)sizeof(struct fuse_file_info);
        // 6) return code (output int)
        argTypes[5] = (1u << ARG_OUTPUT) | (ARG_INT << 16u);
        argTypes[6] = 0; // Null terminator

        ret = rpcRegister((char*)"read", argTypes, watdfs_read);
        if (ret < 0) {
            return ret;
        }
    }

    {   // Register the truncate function.
        int argTypes[4];
        // 1) Path (Input, char array)
        argTypes[0] = (1u << ARG_INPUT) | (1u << ARG_ARRAY) | (ARG_CHAR << 16u) | 1u;
        // 2) New size (Input, long)
        argTypes[1] = (1u << ARG_INPUT) | (ARG_LONG << 16u);
        // 3) Return code (Output, int)
        argTypes[2] = (1u << ARG_OUTPUT) | (ARG_INT << 16u);
        argTypes[3] = 0; // Null terminator

        ret = rpcRegister((char*)"truncate", argTypes, watdfs_truncate);
        if (ret < 0) {
            return ret;
        }
    }

    {   // Register the fsync function.
        int argTypes[4];
        // 1) Path (Input, char array)
        argTypes[0] = (1u << ARG_INPUT) | (1u << ARG_ARRAY)
                    | (ARG_CHAR << 16u) | 1u;
        // 2) File descriptor (Input, int)
        argTypes[1] = (1u << ARG_INPUT) | (ARG_INT << 16u);
        // 3) Return Code (Output, int)
        argTypes[2] = (1u << ARG_OUTPUT) | (ARG_INT << 16u);
        argTypes[3] = 0; // Null terminator

        ret = rpcRegister((char*)"fsync", argTypes, watdfs_fsync);
        if (ret < 0) {
            return ret;
        }
    }


    {   // Register the utimensat function.
        int argTypes[5];
        // 1) Path (Input, char array)
        argTypes[0] = (1u << ARG_INPUT) | (1u << ARG_ARRAY)
                    | (ARG_CHAR << 16u) | 1u;
        // 2) Timestamps (Input, array of struct timespec stored as a char array)
        argTypes[1] = (1u << ARG_INPUT) | (1u << ARG_ARRAY)
                    | (ARG_CHAR << 16u) | (unsigned)(2 * sizeof(struct timespec));
        // 3) Number of timespec elements (Input, int)
        argTypes[2] = (1u << ARG_INPUT) | (ARG_INT << 16u);
        // 4) Return Code (Output, int)
        argTypes[3] = (1u << ARG_OUTPUT) | (ARG_INT << 16u);
        argTypes[4] = 0; // Null terminator

        ret = rpcRegister((char*)"utimensat", argTypes, watdfs_utimensat);
        if (ret < 0) {
            return ret;
        }
    }

    // TODO: Hand over control to the RPC library by calling `rpcExecute`.
    ret = rpcExecute();
    // rpcExecute could fail, so you may want to have debug-printing here, and
    // then you should return.
    if (ret < 0) {
        // # ifdef PRINT_ERR
        // std::cerr << "rpcExecute failed with code: " << ret << std::endl;
        // #endif
        return ret;
    }
    
    return ret;
}

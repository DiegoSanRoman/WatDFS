//
// Starter code for CS 454/654
// You SHOULD change this file
//

#include "watdfs_client.h"
#include "debug.h"
INIT_LOG

#include "rpc.h"
#include <string>

// We define a struct to hold global state data.
struct WatdfsClientGlobalState {
    std::string cache_path;
    time_t      cache_interval;
};


// SETUP AND TEARDOWN
void *watdfs_cli_init(struct fuse_conn_info *conn, const char *path_to_cache,
                      time_t cache_interval, int *ret_code) {
    // TODO: set up the RPC library by calling `rpcClientInit`.
    int rpc_init_result = rpcClientInit();

    // TODO: check the return code of the `rpcClientInit` call
    // `rpcClientInit` may fail, for example, if an incorrect port was exported.
    if (rpc_init_result < 0) {
        #ifdef PRINT_ERR
                std::cerr << "Failed to initialize RPC Client (error code: " 
                        << rpc_init_result << ")" << std::endl;
        #endif
                *ret_code = rpc_init_result;
                return nullptr;
    }

    // It may be useful to print to stderr or stdout during debugging.
    // Important: Make sure you turn off logging prior to submission!
    // One useful technique is to use pre-processor flags like:
    // # ifdef PRINT_ERR
    // std::cerr << "Failed to initialize RPC Client" << std::endl;
    // #endif
    // Tip: Try using a macro for the above to minimize the debugging code.

    // TODO Initialize any global state that you require for the assignment and return it.
    // The value that you return here will be passed as userdata in other functions.
    // In A1, you might not need it, so you can return `nullptr`.
    void *userdata = nullptr;

    // TODO: save `path_to_cache` and `cache_interval` (for A3).
    // We define a small struct to hold future data if needed.
    WatdfsClientGlobalState *state = new WatdfsClientGlobalState();

    if (path_to_cache) {
        state->cache_path = path_to_cache;
    } else {
        state->cache_path = "";
    }
    state->cache_interval = cache_interval;
    userdata = static_cast<void*>(state);

    // TODO: set `ret_code` to 0 if everything above succeeded else some appropriate
    // non-zero value.
    *ret_code = 0;

    // Return pointer to global state data.
    return userdata;
}

void watdfs_cli_destroy(void *userdata) {
    // TODO: clean up your userdata state.
    // Here we cast the userdata pointer back to our global state structure and delete it.
    if (userdata != nullptr) {
        auto *state = static_cast<WatdfsClientGlobalState*>(userdata);
        delete state;
    }
    // TODO: tear down the RPC library by calling `rpcClientDestroy`.
    int destroy_result = rpcClientDestroy();
    if (destroy_result < 0) {
        #ifdef PRINT_ERR
                std::cerr << "Failed to destroy RPC Client (error code: "
                        << destroy_result << ")" << std::endl;
        #endif
    }
}

// GET FILE ATTRIBUTES
int watdfs_cli_getattr(void *userdata, const char *path, struct stat *statbuf) {
    // SET UP THE RPC CALL
    DLOG("watdfs_cli_getattr called for '%s'", path);
    
    // getattr has 3 arguments.
    int ARG_COUNT = 3;

    // Allocate space for the output arguments.
    void **args = new void*[ARG_COUNT];

    // Allocate the space for arg types, and one extra space for the null
    // array element.
    int arg_types[ARG_COUNT + 1];

    // The path has string length (strlen) + 1 (for the null character).
    int pathlen = strlen(path) + 1;

    // Fill in the arguments
    // The first argument is the path, it is an input only argument, and a char
    // array. The length of the array is the length of the path.
    arg_types[0] =
        (1u << ARG_INPUT) | (1u << ARG_ARRAY) | (ARG_CHAR << 16u) | (uint) pathlen;
    // For arrays the argument is the array pointer, not a pointer to a pointer.
    args[0] = (void *)path;

    // The second argument is the stat structure. This argument is an output
    // only argument, and we treat it as a char array. The length of the array
    // is the size of the stat structure, which we can determine with sizeof.
    arg_types[1] = (1u << ARG_OUTPUT) | (1u << ARG_ARRAY) | (ARG_CHAR << 16u) |
                   (uint) sizeof(struct stat); // statbuf
    args[1] = (void *)statbuf;

    // The third argument is the return code, an output only argument, which is
    // an integer.
    // TODO: fill in this argument type.
    arg_types[2] = (1u << ARG_OUTPUT) | (ARG_INT << 16u);

    // The return code is not an array, so we need to hand args[2] an int*.
    // The int* could be the address of an integer located on the stack, or use
    // a heap allocated integer, in which case it should be freed.
    // TODO: Fill in the argument
    int *ret_arg = new int;
    args[2] = (void *)ret_arg;

    // Finally, the last position of the arg types is 0. There is no
    // corresponding arg.
    arg_types[3] = 0;

    // MAKE THE RPC CALL
    int rpc_ret = rpcCall((char *)"getattr", arg_types, args);

    // HANDLE THE RETURN
    // The integer value watdfs_cli_getattr will return.
    int fxn_ret = 0;
    if (rpc_ret < 0) {
        DLOG("getattr rpc failed with error '%d'", rpc_ret);
        // Something went wrong with the rpcCall, return a sensible return
        // value. In this case lets return, -EINVAL
        fxn_ret = -EINVAL;
    } else {
        // Our RPC call succeeded. However, it's possible that the return code
        // from the server is not 0, that is it may be -errno. Therefore, we
        // should set our function return value to the retcode from the server.

        // TODO: set the function return value to the return code from the server.
        fxn_ret = *ret_arg;
    }

    if (fxn_ret < 0) {
        // If the return code of watdfs_cli_getattr is negative (an error), then 
        // we need to make sure that the stat structure is filled with 0s. Otherwise,
        // FUSE will be confused by the contradicting return values.
        memset(statbuf, 0, sizeof(struct stat));
    }

    // Clean up the memory we have allocated.
    delete ret_arg;
    delete []args;

    // Finally return the value we got from the server.
    return fxn_ret;
}

// CREATE, OPEN AND CLOSE
int watdfs_cli_mknod(void *userdata, const char *path, mode_t mode, dev_t dev) {
    // Called to create a file.
    DLOG("watdfs_cli_mknod called for '%s'", path);

    // We'll have 4 arguments for the RPC:
    // 1) path (input, char array)
    // 2) mode_t (input, treated as int)
    // 3) dev_t (input, treated as int)
    // 4) return code (output, int)
    const int ARG_COUNT = 4;

    // Allocate space for the args array
    void **args = new void*[ARG_COUNT];

    // Allocate the space for arg types, plus one extra for the terminating 0
    int arg_types[ARG_COUNT + 1];

    // Compute length of path (including the null terminator)
    int path_len = strlen(path) + 1;

    // 1) Path (input, char array)
    arg_types[0] =
        (1u << ARG_INPUT) | (1u << ARG_ARRAY) | (ARG_CHAR << 16u) | (uint) path_len;
    args[0] = (void *) path;

    // 2) mode_t (input, we consider it as an integer for RPC)
    // Note: mode_t is often 4 bytes, so ARG_INT is typically appropriate.
    arg_types[1] = (1u << ARG_INPUT) | (ARG_INT << 16u);
    args[1] = (void *) &mode;

    // 3) dev_t (input, treated as long here)
    arg_types[2] = (1u << ARG_INPUT) | (ARG_LONG << 16u);   // CHANGED
    args[2] = (void *) &dev;

    // 4) return code (output, int)
    arg_types[3] = (1u << ARG_OUTPUT) | (ARG_INT << 16u);
    int *ret_arg = new int;
    args[3] = (void *) ret_arg;

    // Terminate the arg_types array
    arg_types[4] = 0;

    // Perform the RPC call
    int rpc_ret = rpcCall((char *)"mknod", arg_types, args);

    int fxn_ret = 0;
    if (rpc_ret < 0) {
        DLOG("mknod RPC call failed with error '%d'", rpc_ret);
        // If the rpcCall itself fails, return an appropriate error code
        fxn_ret = -EIO;  // -EIO is I/O error, but choose any sensible error
    } else {
        // If the RPC call succeeds, use the server's return code
        fxn_ret = *ret_arg;  // Could be 0 (success) or -errno
    }

    // Cleanup
    delete ret_arg;
    delete [] args;

    return fxn_ret;
}

int watdfs_cli_open(void *userdata, const char *path,
                    struct fuse_file_info *fi) {
    DLOG("watdfs_cli_open called for '%s'", path);
    // Called during open.
    // You should fill in fi->fh.

    // We'll define three arguments for the RPC call:
    //   1) path    (input, char array)
    //   2) fi      (input+output, char array)
    //   3) retcode (output, int)
    const int ARG_COUNT = 3;

    void **args = new void*[ARG_COUNT];
    int arg_types[ARG_COUNT + 1];

    // 1) path as an input char array
    int path_len = strlen(path) + 1;
    arg_types[0] = (1u << ARG_INPUT) | (1u << ARG_ARRAY) 
                   | (ARG_CHAR << 16u) | (unsigned)path_len;
    args[0] = (void*) path;

    // 2) fi as an in/out char array of size = sizeof(struct fuse_file_info)
    //    The server can read fi->flags from here and write fi->fh back.
    arg_types[1] = (1u << ARG_INPUT) | (1u << ARG_OUTPUT) 
                   | (1u << ARG_ARRAY) | (ARG_CHAR << 16u)
                   | (unsigned)sizeof(struct fuse_file_info);
    args[1] = (void*) fi;

    // 3) return code (output int)
    arg_types[2] = (1u << ARG_OUTPUT) | (ARG_INT << 16u);
    int *ret_code = new int;
    args[2] = (void*) ret_code;

    // End of the arg_types array
    arg_types[3] = 0;

    // Make the RPC call
    int rpc_ret = rpcCall((char*)"open", arg_types, args);

    int fxn_ret = 0;
    if (rpc_ret < 0) {
        // If the RPC call itself fails
        DLOG("open RPC call failed with error '%d'", rpc_ret);
        fxn_ret = -EIO;
    } else {
        // Server’s return code is 0 or -errno
        fxn_ret = *ret_code;
    }

    // If fxn_ret == 0, then fi->fh should now be set by the server
    // (because fi was passed in/out). Nothing else is required here.

    delete ret_code;
    delete [] args;
    return fxn_ret;
}

int watdfs_cli_release(void *userdata, const char *path, struct fuse_file_info *fi) {
    DLOG("watdfs_cli_release called for '%s'", path);

    // The harness expects 3 arguments:
    //  1) path (input, char array)
    //  2) fi   (input, char array, size = sizeof(fuse_file_info))
    //  3) retcode (output, int)
    const int ARG_COUNT = 3;
    void **args = new void*[ARG_COUNT];
    int arg_types[ARG_COUNT + 1];

    // 1) path
    int path_len = strlen(path) + 1;
    arg_types[0] = (1u << ARG_INPUT) | (1u << ARG_ARRAY)
                   | (ARG_CHAR << 16u) | (unsigned)path_len;
    args[0] = (void*) path;

    // 2) fi as input, char array
    arg_types[1] = (1u << ARG_INPUT) | (1u << ARG_ARRAY)
                   | (ARG_CHAR << 16u) | (unsigned)sizeof(struct fuse_file_info);
    args[1] = (void*) fi;

    // 3) retcode (output, int)
    arg_types[2] = (1u << ARG_OUTPUT) | (ARG_INT << 16u);
    int *ret_code = new int;
    args[2] = (void*) ret_code;

    // end
    arg_types[3] = 0;

    // Call RPC
    int rpc_ret = rpcCall((char*)"release", arg_types, args);

    int fxn_ret = 0;
    if (rpc_ret < 0) {
        fxn_ret = -EIO;
    } else {
        fxn_ret = *ret_code;  // 0 or -errno
    }

    // Cleanup
    delete ret_code;
    delete[] args;

    return fxn_ret;
}

// READ AND WRITE DATA
int watdfs_cli_read(void *userdata, const char *path, char *buf, size_t size,
                    off_t offset, struct fuse_file_info *fi) {
    // Read size amount of data at offset of file into buf.

    // Remember that size may be greater than the maximum array size of the RPC
    // library.

    // We will handle potentially large reads by splitting them into multiple
    // smaller RPC calls. Each call will read up to MAX_ARRAY_LEN bytes.

    // The approach:
    // 1. Loop until we've read `size` bytes or encounter an error/EOF.
    // 2. Each iteration, we request a chunk from the server (up to MAX_ARRAY_LEN).
    // 3. We gather the data in `buf` at the correct offset.
    // 4. If the server returns 0, we've hit EOF; if it returns < 0, it's an error.
    // 5. Otherwise, we add the chunk size read to our total.

    size_t total_read = 0;
    off_t current_offset = offset;
    int fxn_ret = 0;

    while (total_read < size) {
        // Decide how big this chunk is
        size_t chunk_len = size - total_read;
        if (chunk_len > MAX_ARRAY_LEN) {
            chunk_len = MAX_ARRAY_LEN;
        }

        // We'll have 6 arguments: path, buf, size, offset, fi, retcode.
        const int ARG_COUNT = 6;
        void **args = new void*[ARG_COUNT];
        int arg_types[ARG_COUNT + 1];

        // 0) path (input, char array)
        int path_len = strlen(path) + 1;
        arg_types[0] = (1u << ARG_INPUT) | (1u << ARG_ARRAY) 
                    | (ARG_CHAR << 16) | (unsigned) path_len;
        args[0] = (void*) path;

        // 1) buf (output, char array) - write into buf + total_read
        arg_types[1] = (1u << ARG_OUTPUT) | (1u << ARG_ARRAY) 
                    | (ARG_CHAR << 16) | (unsigned) chunk_len;
        args[1] = (void*) (buf + total_read);

        // 2) size (input, long) - the chunk size
        long chunk_len_long = (long) chunk_len;  // so we have a proper "long"
        arg_types[2] = (1u << ARG_INPUT) | (ARG_LONG << 16);
        args[2] = (void*) &chunk_len_long;

        // 3) offset (input, long)
        // off_t is typically 64 bits, so passing its address should be fine
        arg_types[3] = (1u << ARG_INPUT) | (ARG_LONG << 16);
        args[3] = (void*) &current_offset;

        // 4) fi (input, char array) - pass the entire fuse_file_info
        arg_types[4] = (1u << ARG_INPUT) | (1u << ARG_ARRAY) 
                    | (ARG_CHAR << 16) | (unsigned) sizeof(struct fuse_file_info);
        args[4] = (void*) fi;

        // 5) retcode (output, int)
        arg_types[5] = (1u << ARG_OUTPUT) | (ARG_INT << 16);
        int *read_ret = new int;
        args[5] = (void*) read_ret;

        // Mark the end
        arg_types[6] = 0;

        // Make the RPC call
        int rpc_ret = rpcCall((char*)"read", arg_types, args);

        // Check for RPC layer error
        if (rpc_ret < 0) {
            fxn_ret = -EIO;
            delete read_ret;
            delete[] args;
            break;
        }

        // Check server return code
        if (*read_ret < 0) {
            // Negative => -errno
            fxn_ret = *read_ret;
            delete read_ret;
            delete[] args;
            break;
        }

        // 0 => EOF
        if (*read_ret == 0) {
            delete read_ret;
            delete[] args;
            break;
        }

        // Otherwise, we read some data
        int bytes_read = *read_ret;
        total_read += bytes_read;
        current_offset += bytes_read;

        delete read_ret;
        delete[] args;

        // If we got fewer bytes than requested, also EOF
        if (bytes_read < (int) chunk_len) {
            break;
        }
    }

    // If fxn_ret is still 0, we return total bytes read.
    if (fxn_ret == 0) {
        fxn_ret = (int) total_read;
    }
    return fxn_ret;
}

int watdfs_cli_write(void *userdata, const char *path, const char *buf,
                     size_t size, off_t offset, struct fuse_file_info *fi) {
    // Write size amount of data at offset of file from buf.

    // Remember that size may be greater than the maximum array size of the RPC
    // library.

    // Similar to read, we split the write request into multiple chunks if `size`
    // exceeds MAX_ARRAY_LEN. We'll accumulate how many bytes we've successfully written.
    size_t total_written = 0;
    off_t current_offset = offset;
    int fxn_ret = 0;

    while (total_written < size) {
        // Decide how large this chunk is
        size_t chunk_len = size - total_written;
        if (chunk_len > MAX_ARRAY_LEN) {
            chunk_len = MAX_ARRAY_LEN;
        }

        // The write protocol expects 6 arguments in this order:
        //   1) path (input, char array)
        //   2) buf  (input, char array)
        //   3) size (input, long)
        //   4) offset (input, long)
        //   5) fi   (input, char array)
        //   6) retcode (output, int)

        const int ARG_COUNT = 6;
        void **args = new void*[ARG_COUNT];
        int arg_types[ARG_COUNT + 1];

        // 1) path (input char array)
        int path_len = strlen(path) + 1;
        arg_types[0] = (1u << ARG_INPUT) | (1u << ARG_ARRAY)
                       | (ARG_CHAR << 16u) | (unsigned)path_len;
        args[0] = (void*) path;

        // 2) buf (input char array)
        // We pass the chunk we want to write: buf + total_written
        arg_types[1] = (1u << ARG_INPUT) | (1u << ARG_ARRAY)
                       | (ARG_CHAR << 16u) | (unsigned)chunk_len;
        args[1] = (void*) (buf + total_written);

        // 3) size (input, long) – we pass this chunk_len as a long
        long chunk_len_long = (long)chunk_len;
        arg_types[2] = (1u << ARG_INPUT) | (ARG_LONG << 16u);
        args[2] = (void*) &chunk_len_long;

        // 4) offset (input, long)
        arg_types[3] = (1u << ARG_INPUT) | (ARG_LONG << 16u);
        args[3] = (void*) &current_offset;

        // 5) fi (input, char array) – entire fuse_file_info
        arg_types[4] = (1u << ARG_INPUT) | (1u << ARG_ARRAY)
                       | (ARG_CHAR << 16u) | (unsigned)sizeof(struct fuse_file_info);
        args[4] = (void*) fi;

        // 6) retcode (output, int) – number of bytes written or -errno
        arg_types[5] = (1u << ARG_OUTPUT) | (ARG_INT << 16u);
        int *write_ret = new int;
        args[5] = (void*) write_ret;

        // End of arg_types
        arg_types[6] = 0;

        // Perform the RPC
        int rpc_ret = rpcCall((char*)"write", arg_types, args);
        if (rpc_ret < 0) {
            // If the RPC layer itself fails
            DLOG("write RPC call failed with error '%d'", rpc_ret);
            fxn_ret = -EIO;
            delete write_ret;
            delete[] args;
            break;
        }

        // The server's return code is the number of bytes written, or negative errno
        if (*write_ret < 0) {
            fxn_ret = *write_ret;  // e.g. -EACCES
            delete write_ret;
            delete[] args;
            break;
        }

        // If server wrote 0 bytes => no more can be written (disk full, etc.)
        if (*write_ret == 0) {
            delete write_ret;
            delete[] args;
            break;
        }

        // Otherwise, accumulate how many bytes we wrote
        int bytes_written_this_time = *write_ret;
        total_written += bytes_written_this_time;
        current_offset += bytes_written_this_time;

        delete write_ret;
        delete[] args;

        // If fewer bytes were written than requested => likely out of space
        if (bytes_written_this_time < (int)chunk_len) {
            break;
        }
    }

    // If fxn_ret is still 0, we successfully wrote total_written bytes
    if (fxn_ret == 0) {
        fxn_ret = (int) total_written;
    }

    return fxn_ret;
}

int watdfs_cli_truncate(void *userdata, const char *path, off_t newsize) {
    // Change the file size to newsize.

    DLOG("watdfs_cli_truncate called for '%s' with new size: %ld", path, (long)newsize);

    // We'll define three arguments for the "truncate" RPC:
    // 1) path (input, char array)
    // 2) newsize (input, long)
    // 3) return code (output, int)

    const int ARG_COUNT = 3;
    void **args = new void*[ARG_COUNT];
    int arg_types[ARG_COUNT + 1];

    // 1) path as input char array
    int path_len = strlen(path) + 1;
    arg_types[0] = (1u << ARG_INPUT) | (1u << ARG_ARRAY)
                   | (ARG_CHAR << 16u) | (uint) path_len;
    args[0] = (void*) path;

    // 2) newsize as input long
    arg_types[1] = (1u << ARG_INPUT) | (ARG_LONG << 16u);
    args[1] = (void*) &newsize;

    // 3) return code (output, int)
    arg_types[2] = (1u << ARG_OUTPUT) | (ARG_INT << 16u);
    int *ret_code = new int;
    args[2] = (void*) ret_code;

    // End of arg_types
    arg_types[3] = 0;

    // Make the RPC call
    int rpc_ret = rpcCall((char*)"truncate", arg_types, args);

    int fxn_ret = 0;
    if (rpc_ret < 0) {
        DLOG("truncate RPC call failed with error '%d'", rpc_ret);
        // If the call fails at the RPC layer, return a generic I/O error
        fxn_ret = -EIO;
    } else {
        // If the RPC call was successful, use the server's return code
        fxn_ret = *ret_code;  // Could be 0 or -errno
    }

    // Clean up
    delete ret_code;
    delete [] args;

    return fxn_ret;
}

int watdfs_cli_fsync(void *userdata, const char *path,
                     struct fuse_file_info *fi) {
    // Force a flush of file data.

    DLOG("watdfs_cli_fsync called for '%s'", path);

    // We'll define three arguments for the "fsync" RPC:
    // 1) path (input, char array)
    // 2) server file handle (input, int), which we get from fi->fh
    // 3) return code (output, int)

    const int ARG_COUNT = 3;
    void **args = new void*[ARG_COUNT];
    int arg_types[ARG_COUNT + 1];

    // 1) path as input char array
    int path_len = strlen(path) + 1;
    arg_types[0] = (1u << ARG_INPUT) | (1u << ARG_ARRAY)
                   | (ARG_CHAR << 16u) | (uint)path_len;
    args[0] = (void*) path;

    // 2) server file handle as input int
    arg_types[1] = (1u << ARG_INPUT) | (ARG_INT << 16u);
    int server_fh = (int) fi->fh; // cast the 64-bit fh to int
    args[1] = (void*) &server_fh;

    // 3) return code (output, int)
    arg_types[2] = (1u << ARG_OUTPUT) | (ARG_INT << 16u);
    int *ret_code = new int;
    args[2] = (void*) ret_code;

    // End of the arg_types array
    arg_types[3] = 0;

    // Make the RPC call
    int rpc_ret = rpcCall((char*)"fsync", arg_types, args);

    int fxn_ret = 0;
    if (rpc_ret < 0) {
        // If the RPC call itself fails
        DLOG("fsync RPC call failed with error '%d'", rpc_ret);
        fxn_ret = -EIO;  // generic I/O error
    } else {
        // If the RPC call was successful, the return code is from the server
        fxn_ret = *ret_code;
    }

    // Clean up
    delete ret_code;
    delete [] args;

    return fxn_ret;
}

// CHANGE METADATA
int watdfs_cli_utimensat(void *userdata, const char *path,
                       const struct timespec ts[2]) {
    // Change file access and modification times.
    
    DLOG("watdfs_cli_utimensat called for '%s'", path);

    // We'll define four arguments for the "utimensat" RPC:
    // 1) path (input, char array)
    // 2) timespec data (input, treated as a char array of length 2 * sizeof(timespec))
    // 3) number of timespec elements (input, int) — in this case, 2
    // 4) return code (output, int)

    const int ARG_COUNT = 4;
    void **args = new void*[ARG_COUNT];
    int arg_types[ARG_COUNT + 1];

    // 1) path as input char array
    int path_len = strlen(path) + 1;
    arg_types[0] = (1u << ARG_INPUT) | (1u << ARG_ARRAY)
                   | (ARG_CHAR << 16u) | (uint) path_len;
    args[0] = (void*) path;

    // 2) timespec data as input char array
    // We'll store the two timespec structs in a small heap buffer
    size_t timespec_array_size = 2 * sizeof(struct timespec);
    arg_types[1] = (1u << ARG_INPUT) | (1u << ARG_ARRAY)
                   | (ARG_CHAR << 16u) | (uint) timespec_array_size;

    struct timespec *ts_copy = new struct timespec[2];
    ts_copy[0] = ts[0];
    ts_copy[1] = ts[1];
    args[1] = (void*) ts_copy;

    // 3) number of timespec elements (input, int)
    arg_types[2] = (1u << ARG_INPUT) | (ARG_INT << 16u);
    int num_ts = 2;
    args[2] = (void*) &num_ts;

    // 4) return code (output, int)
    arg_types[3] = (1u << ARG_OUTPUT) | (ARG_INT << 16u);
    int *ret_code = new int;
    args[3] = (void*) ret_code;

    // End of arg_types
    arg_types[4] = 0;

    // Make the RPC call
    int rpc_ret = rpcCall((char*)"utimensat", arg_types, args);

    int fxn_ret = 0;
    if (rpc_ret < 0) {
        DLOG("utimensat RPC call failed with error '%d'", rpc_ret);
        // If the call fails at the RPC layer, return a generic I/O error
        fxn_ret = -EIO;
    } else {
        // Use the server's return code
        fxn_ret = *ret_code;  // Could be 0 or -errno
    }

    // Clean up
    delete[] ts_copy;
    delete ret_code;
    delete[] args;

    return fxn_ret;
}

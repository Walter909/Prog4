#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include "libDisk.h"
#include "Errors.h"

/* This function opens a regular UNIX file and designates the first
 * nBytes of it as space for the emulated disk. nBytes should be a
 * number that is evenly divisible by the block size. If nBytes > 0
 * and there is already a file by the given filename, that disk is
 * resized to nBytes, and that file’s contents may be overwritten.
 * If nBytes is 0, an existing disk is opened, and should not be
 * overwritten. There is no requirement to maintain integrity of
 * any content beyond nBytes. Errors must be returned for any other
 * failures, as defined by your own error code system.  */
int openDisk(char *filename, int nBytes) {
    int disk;

    if (nBytes % BLOCK_SIZE != 0) {
        // nBytes should be evenly divisible by the block size
        nBytes = DEFAULT_DISK_SIZE;
    }

    if (filename == NULL) {
        filename = DEFAULT_DISK_NAME;
    }

    if (nBytes > 0) {
        // Open the file in read-write mode
        disk = open(filename, O_RDWR | O_CREAT);
        if (disk == -1) {
            // Failed to open the file
            close(disk);
            return OPENDISK_FAILED;
        }

        // Resize the disk if the file already exists
        if (access(filename, F_OK) == 0) {
            if (ftruncate(disk, nBytes) == -1) {
                // Failed to resize the disk
                close(disk);
                return OPENDISK_FAILED;
            }
        }
    } else {
        // Open the file in read-write mode without truncating it
        if ((disk = open(filename, O_RDWR)) == -1) {
            // Failed to open the file
            close(disk);
            return OPENDISK_FAILED;
        }
    }

    return disk; // the fd represents the disk Number
}

/* readBlock() reads an entire block of BLOCKSIZE bytes from
 * the open disk (identified by ‘disk’) and copies the result
 * into a local buffer (must be at least of BLOCKSIZE bytes).
 * The bNum is a logical block number, which must be translated
 * into a byte offset within the disk. The translation from logical
 * to physical block is straightforward: bNum=0 is the very first
 * byte of the file. bNum=1 is BLOCKSIZE bytes into the disk,
 * bNum=n is n*BLOCKSIZE bytes into the disk. On success, it
 * returns 0. Errors must be returned if ‘disk’ is not available
 * (i.e. hasn’t been opened) or for any other failures, as defined
 * by your own error code system. */
int readBlock(int disk, int bNum, void *block) {

    off_t offset = bNum * BLOCK_SIZE;
    if (lseek(disk, offset, SEEK_SET) == -1) {
        // Failed to seek to the specified block
        return READBLK_FAILED;
    }

    ssize_t bytesRead = read(disk, block, BLOCK_SIZE);
    if (bytesRead == -1) {
        // Failed to read the block
        return READBLK_FAILED;
    }

    return 0; // Return success
}

/* writeBlock() takes disk number ‘disk’ and logical block number ‘bNum’
 * and writes the content of the buffer ‘block’ to that location. BLOCKSIZE
 * bytes will be written from ‘block’ regardless of its actual size.
 * The disk must be open. Just as in readBlock(), writeBlock() must
 * translate the logical block bNum to the correct byte position in
 * the file. On success, it returns 0. Errors must be returned if ‘disk’
 * is not available (i.e. hasn’t been opened) or for any other failures,
 * as defined by your own error code system. */
int writeBlock(int disk, int bNum, void *block) {
    if (lseek(disk, bNum * BLOCK_SIZE, SEEK_SET) == -1) {
        //disk offset failed
        return WRITEBLK_FAILED;
    }

    if (write(disk, block, BLOCK_SIZE) == -1) {
        //write error
        return WRITEBLK_FAILED;
    }

    return 0; // Success
}

/* closeDisk() takes a disk number ‘disk’ and makes the disk closed to
 * further I/O; i.e. any subsequent reads or writes to a closed disk
 * should return an error. Closing a disk should also close the underlying
 * file, committing any writes being buffered by the real OS. */
void closeDisk(int disk) {
    close(disk);
}



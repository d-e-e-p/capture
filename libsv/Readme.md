# LibSV software library

## About 

LibSV (Streamlined V4L2 Library) is a software library that provides simplified and consistent control of sensors exposed using V4L2 API.

Main features of LibSV are:
- ease of use - simple interface that wraps the complex and low-level V4L2 API 
- consistency - consistent interface across all supported platforms

## Supported platforms

- Jetson TX2
- Jetson Xavier
- DragonBoard 410c

## Interface

Interfaces for following languages are provided:

- C++
- C

LibSV interface is documented in the file `/include/sv/sv.h`.

## Examples

### Available examples

- acquire_image - basic C++ example that configures sensors and acquires images
- save_image - C++ example that saves acquired images as raw files
- display_image - C++ example that displays acquired images using OpenCV
- acquire_image_c - basic C example that configures sensor and acquires images
- save_image_c - C examples that saves acquired images as raw files
- display_image_c - C examples that displays acquired images using OpenCV
                  - note that the OpenCV C interface is scheduled for removal from OpenCV API

### Executable examples

Executable examples are located in `/bin` folder.

### Example sources

Source code for examples is located in `/examples` folder.

Examples can be built using the `/examples/bin/build.sh` script.

Prerequisites for building:

- g++ compiler
- OpenCV 3.3.1

## Platform-specific processing

Most user-space applications cannot handle raw data provided by image sensors. Moreover, different image pipelines on different platforms produce different raw outputs. Because of that, LibSV can reformat the raw data it acquires from image sensor into a raw format that most applications can handle and that is consistent across platforms. Note that using this feature may decrease performance. See examples for reference.

#ifndef INCLUDE_SV_ENUM_H
#define INCLUDE_SV_ENUM_H

enum SV_ENUMS
{
    // API
    SV_API_BUFFERCOUNT = 1,         /**< Enum for number of allocated v4l2 buffers. Default value is 10 */
    SV_API_FETCHBLOCKING,           /**< Enum for GetImage modes. Default value is 1 (true) */
    SV_API_BLOCKINGTIMEOUT,         /**< Enum for GetImage timeout. Default value is 0 ms (indefinitely) */
    SV_API_PREVENT_TEARING,         /**< Enum for toggling image tearing prevention mechanism. Default value is 1 (true) */
    SV_API_PREVENT_TEARING_TIMEOUT, /**< Enum for tearing prevention mechanism timeout. Units in ms. 0 ms is indefinite wait. */

    // V4L2
    SV_V4L2_IMAGEFORMAT = 10,       /**< Enum for image format. This is handled by V4L2. */
    SV_V4L2_FRAMESIZE,              /**< Enum for frame size. This is handled by V4L2. */
    SV_V4L2_FRAMEINTERVAL,          /**< Enum for frame interval. This is handled by V4L2.*/

    //Additional added by -s on feb 2020
    // WARNING: might change with different camera controllers
    SV_V4L2_BUFFER_COUNT                    =1 ,
    SV_V4L2_FETCH_BLOCKING                  =2 ,
    SV_V4L2_BLOCKING_TIMEOUT                =3 ,
    SV_V4L2_PREVENT_TEARING                 =4 ,
    SV_V4L2_PREVENT_TEARING_TIMEOUT         =5 ,
    SV_V4L2_IMAGE_FORMAT                    =10 ,
    SV_V4L2_FRAME_SIZE                      =11 ,
    SV_V4L2_FRAME_INTERVAL                  =12 ,
    SV_V4L2_GROUP_HOLD                      =10100739                ,
    SV_V4L2_SENSOR_MODE                     =10100744                ,
    SV_V4L2_GAIN                            =10100745                ,
    SV_V4L2_EXPOSURE                        =10100746                ,
    SV_V4L2_FRAME_RATE                      =10100747                ,
    SV_V4L2_BYPASS_MODE                     =10100836                ,
    SV_V4L2_OVERRIDE_ENABLE                 =10100837                ,
    SV_V4L2_HEIGHT_ALIGN                    =10100838                ,
    SV_V4L2_SIZE_ALIGN                      =10100839                ,
    SV_V4L2_WRITE_ISP_FORMAT                =10100840                ,
    SV_V4L2_LOW_LATENCY_MODE                =10100845                ,
    SV_V4L2_TEST_PATTERN                    =10100847                ,
    SV_V4L2_STREAMING_MODE                  =10100848                ,
    SV_V4L2_OPERATION_MODE                  =10100849                ,
    SV_V4L2_BLACK_LEVEL                     =10100852                ,
    SV_V4L2_GLOBAL_SHUTTER_MODE             =10100859                ,
    SV_V4L2_SENSOR_MODES                    =10100866                ,

};

enum SV_PLATFORM_PROCESSING
{
    SV_ALGORITHM_AUTODETECT         /**< Processing algorithm is detected automatically based on platform and image format */
};

#endif /*INCLUDE_SV_ENUM_H*/

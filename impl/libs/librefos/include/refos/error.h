/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#ifndef _REFOS_ERROR_H_
#define _REFOS_ERROR_H_

/* Include Kconfig variables. */
#include <autoconf.h>

/*! @file
    @brief RefOS Error codes.
    
    Shared error codes for RefOS methods.
*/

enum refos_error {
    /*! @brief There was no error. */
    ESUCCESS = 0,
    
    /*! @brief Ran out of heap, cslots or untyped memory. */
    ENOMEM,
    
    /*! @brief An internal error occured with the service. */
    EINVALID,
    
    /*! @brief The given client capability is invalid. */
    EUNKNOWNCLIENT,
    
    /*! @brief A given parameter was invalid. */
    EINVALIDPARAM,
    
    /*! @brief The server name was not found by the naming service. */
    ESERVERNOTFOUND,
    
    /*! @brief The given window capability is invalid. */
    EINVALIDWINDOW,
    
    /*! @brief The server is currently unavailable; try again later. */
    ESERVICEUNAVAILABLE,
    
    /*! @brief Insufficient permission for the requested operation. */
    EACCESSDENIED,
    
    /*! @brief Occurs when a pager service tries to map a frame where there already is one. */
    EUNMAPFIRST,
    
    /*! @brief The file name was not found on the dataspace server. */
    EFILENOTFOUND,
    
    /*! @brief End of file was reached. */
    EENDOFFILE,
    
    /*! @brief The device was not found on the system. */
    EDEVICENOTFOUND,
   
    /*! @brief The operation requires access to the param buffer but no param buffer exists */
    ENOPARAMBUFFER,

    /*! @brief This feature has not been implemented. */
    EUNIMPLEMENTED,

    /*! @brief Request has been delegated to another server. */
    EDELEGATED
};

#ifndef __defined_err_t__
    typedef enum refos_error refos_err_t;
    #define __defined_err_t__
#endif

/*! @brief Helper function which returns the associated string with a RefOS error number. Useful for
           printing debugging information.
    @param err The RefOS error.
    @return Static string containing the error variable enum name. (No ownership transfer)
*/
static inline const char*
refos_error_str(refos_err_t err)
{
    switch (err) {
        case ESUCCESS:
            return "ESUCCESS";
        case ENOMEM:
            return "ENOMEM";
        case EINVALID:
            return "EINVALID";
        case EUNKNOWNCLIENT:
            return "EUNKNOWNCLIENT";
        case EINVALIDPARAM:
            return "EINVALIDPARAM";
        case ESERVERNOTFOUND:
            return "ESERVERNOTFOUND";
        case EINVALIDWINDOW:
            return "EINVALIDWINDOW";
        case ESERVICEUNAVAILABLE:
            return "ESERVICEUNAVAILABLE";
        case EACCESSDENIED:
            return "EACCESSDENIED";
        case EUNMAPFIRST:
            return "EUNMAPFIRST";
        case EFILENOTFOUND:
            return "EFILENOTFOUND";
        case EENDOFFILE:
            return "EENDOFFILE";
        case EDEVICENOTFOUND:
            return "EDEVICENOTFOUND";
        case ENOPARAMBUFFER:
            return "ENOPARAMBUFFER";
        case EUNIMPLEMENTED:
            return "EUNIMPLEMENTED";
        case EDELEGATED:
            return "EDELEGATED";
        default:
            return "EUNKNOWNERROR";
    }
    return (const char*) 0;
}

#include <sel4/errors.h>

/*! @brief Helper function to return the enum name string associated with a seL4 error.
    @param sel4err The seL4 error number.
    @return Static str containing the string name of the given error code. (No ownership transfer)
*/
static inline const char*
seL4_error_str(int sel4err)
{
    switch (sel4err) {
        case seL4_NoError:
            return "seL4_NoError";
        case seL4_InvalidArgument:
            return "seL4_InvalidArgument";
        case seL4_InvalidCapability:
            return "seL4_InvalidCapability";
        case seL4_IllegalOperation:
            return "seL4_IllegalOperation";
        case seL4_RangeError:
            return "seL4_RangeError";
        case seL4_AlignmentError:
            return "seL4_AlignmentError";
        case seL4_FailedLookup:
            return "seL4_FailedLookup";
        case seL4_TruncatedMessage:
            return "seL4_TruncatedMessage";
        case seL4_DeleteFirst:
            return "seL4_DeleteFirst";
        case seL4_RevokeFirst:
            return "seL4_RevokeFirst";
        case seL4_NotEnoughMemory:
            return "seL4_NotEnoughMemory";
        default:
            return "Unknown seL4 Error.";
    }
};

extern refos_err_t _refos_errno;

#if CONFIG_REFOS_HALT_ON_ERRNO
    #define REFOS_SET_ERRNO(x) _refos_errno = x; if (_refos_errno != ESUCCESS) \
        {\
            printf("REFOS call generated error in file %s line %d", __FILE__, __LINE__);\
            assert(!"Halt because REFOS_HALT_ON_ERRNO is enabled.");\
            while(1);\
        }
    #define REFOS_GET_ERRNO() (_refos_errno)
#else
    #define REFOS_SET_ERRNO(x) (_refos_errno = x)
    #define REFOS_GET_ERRNO() (_refos_errno)
#endif
#define ROS_ERRNO REFOS_GET_ERRNO
#define ROS_SET_ERRNO REFOS_SET_ERRNO

#endif /* _REFOS_ERROR_H_ */

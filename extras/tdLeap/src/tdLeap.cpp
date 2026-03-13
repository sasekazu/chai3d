//===========================================================================
/*
    Software License Agreement (BSD License)
    Copyright (c) 2003-2024, CHAI3D
    (www.chai3d.org)

    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions
    are met:

    * Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.

    * Redistributions in binary form must reproduce the above
    copyright notice, this list of conditions and the following
    disclaimer in the documentation and/or other materials provided
    with the distribution.

    * Neither the name of CHAI3D nor the names of its contributors may
    be used to endorse or promote products derived from this software
    without specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
    "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
    LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
    FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
    COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
    INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
    BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
    LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
    CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
    LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
    ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
    POSSIBILITY OF SUCH DAMAGE. 

    \author     Sebastien Grange
*/
//===========================================================================

//---------------------------------------------------------------------------
#include "tdLeap.h"
//---------------------------------------------------------------------------
#include "chai3d.h"
using namespace chai3d;
//---------------------------------------------------------------------------
#include <cstring>
//---------------------------------------------------------------------------

// Constants
#define DEG_TO_RAD (3.14159265358979323846 / 180.0)

//=============================================================================
// GLOBAL VARIABLES
//=============================================================================

// LeapC connection handle
static LEAP_CONNECTION _connection = NULL;

// internal tracking data
static LEAP_TRACKING_EVENT _lastTrackingEvent;
static int64_t _lastFrameID = -1;
static bool _isConnected = false;
static bool _hasTracking = false;


//=============================================================================
// INTERNAL HELPER FUNCTIONS
//=============================================================================

void
_sleepMs(const unsigned int a_interval)
{
#if defined(WIN32) | defined(WIN64)
    Sleep(a_interval);
#endif

#if defined(LINUX) | defined (MACOSX)
    struct timespec t;
    t.tv_sec  = a_interval/1000;
    t.tv_nsec = (a_interval-t.tv_sec*1000)*1000000;
    nanosleep (&t, NULL);
#endif
}


void
_pollConnection()
{
    if (!_connection)
        return;

    LEAP_CONNECTION_MESSAGE msg;
    unsigned int timeout = 0;
    
    while (LeapPollConnection(_connection, timeout, &msg) == eLeapRS_Success)
    {
        switch (msg.type)
        {
            case eLeapEventType_Connection:
                _isConnected = true;
                break;

            case eLeapEventType_ConnectionLost:
                _isConnected = false;
                _hasTracking = false;
                break;

            case eLeapEventType_Tracking:
                _lastTrackingEvent = *msg.tracking_event;
                _lastFrameID = msg.tracking_event->info.frame_id;
                _hasTracking = true;
                break;

            case eLeapEventType_Device:
                // Device connected
                break;

            case eLeapEventType_DeviceLost:
                // Device disconnected
                break;

            default:
                break;
        }
    }
}


//==========================================================================
// PUBLIC API FUNCTIONS
//==========================================================================

//==========================================================================
/*!
    Retrieves the number of devices of type Leap Motion.

    \fn     int __FNCALL tdLeapGetNumDevices()

    \return Returns the number of devices found, -1 if controller is not ready.
*/
//==========================================================================
int __FNCALL tdLeapGetNumDevices()
{
    if (!_connection || !_isConnected)
    {
        return (-1);
    }

    // Poll for any new events
    _pollConnection();

    // In LeapC, device count is obtained differently
    // Return 1 if connected, 0 otherwise (simplified approach)
    return _isConnected ? 1 : 0;
}


//==========================================================================
/*!
    Open a connection to the device selected.

    \fn     int __FNCALL tdLeapOpen()

     \return Return 0 if success, otherwise -1.
*/
//==========================================================================
int __FNCALL tdLeapOpen()
{
    // Create connection if it doesn't exist
    if (!_connection)
    {
        LEAP_CONNECTION_CONFIG config;
        config.size = sizeof(config);
        config.flags = 0;
        config.server_namespace = NULL;

        if (LeapCreateConnection(&config, &_connection) != eLeapRS_Success)
        {
            return (-1);
        }

        if (LeapOpenConnection(_connection) != eLeapRS_Success)
        {
            LeapDestroyConnection(_connection);
            _connection = NULL;
            return (-1);
        }

        // Wait for connection
        for (int i = 0; i < 100; i++)
        {
            _pollConnection();
            if (_isConnected)
                break;
            _sleepMs(10);
        }

        if (!_isConnected)
        {
            LeapDestroyConnection(_connection);
            _connection = NULL;
            return (-1);
        }
    }

    // success
    return (0);
}


//==========================================================================
/*!
    Closes a connection to the device.

    \fn     int __FNCALL tdLeapClose()
    
    \return Return 0 if success, otherwise -1.
*/
//==========================================================================
int __FNCALL tdLeapClose()
{  
    if (_connection)
    {
        LeapCloseConnection(_connection);
        LeapDestroyConnection(_connection);
        _connection = NULL;
        _isConnected = false;
        _hasTracking = false;
    }
  
    // success
    return (0);
}


//==========================================================================
/*!
    Update the global frame to the latest available.

    \fn       bool __FNCALL tdLeapUpdate()

    \return Return __true__ on success, __false__ otherwise.
*/
//==========================================================================
bool __FNCALL tdLeapUpdate()
{
    // check if device is physically available
    if (!_connection || !_isConnected)
    {
        return (false);
    }

    // Poll for new tracking data
    _pollConnection();

    // Return true if we have tracking data
    return _hasTracking;
}


//==========================================================================
/*!
    Return the hand position from the tracking data retrieved by the last
    \ref tdLeapUpdate() call.

    \fn       bool __FNCALL tdLeapGetPosition(cVector3d a_position[2])

    \param a_position   Returned hand position.

    \return Return __true__ on success, __false__ otherwise.
*/
//==========================================================================
bool __FNCALL tdLeapGetPosition(cVector3d a_position[2])
{
    // check if device is physically available
    if (!_connection || !_isConnected || !_hasTracking)
    {
        return (false);
    }

    // Initialize positions to zero
    a_position[0].set(0.0, 0.0, 0.0);
    a_position[1].set(0.0, 0.0, 0.0);

    // Process hands (index 0 = right hand, index 1 = left hand)
    for (uint32_t h = 0; h < _lastTrackingEvent.nHands && h < 2; h++)
    {
        LEAP_HAND* hand = &_lastTrackingEvent.pHands[h];
        
        // Determine hand index (0 for right, 1 for left)
        int handIndex = (hand->type == eLeapHandType_Right) ? 0 : 1;
        
        // Get palm position in millimeters and convert to meters
        // Leap coordinate system: x=right, y=up, z=towards user
        // CHAI3D coordinate system: x=towards user, y=right, z=up
        // Transform: CHAI3D.x = Leap.z, CHAI3D.y = Leap.x, CHAI3D.z = Leap.y
        a_position[handIndex].set(
            hand->palm.position.z * 1e-3,  // towards user (from Leap z)
            hand->palm.position.x * 1e-3,  // right (from Leap x)
            hand->palm.position.y * 1e-3   // up (from Leap y)
        );
    }

    return (true);
}


//==========================================================================
/*!
    Return the hand rotation from the tracking data retrieved by the last
    \ref tdLeapUpdate() call.

    \fn       bool __FNCALL tdLeapGetRotation(cMatrix3d a_rotation[2])

    \param a_rotation   Returned hand rotation.

    \return Return __true__ on success, __false__ otherwise.
*/
//==========================================================================
bool __FNCALL tdLeapGetRotation(cMatrix3d a_rotation[2])
{
    if (!_connection || !_isConnected || !_hasTracking)
    {
        return false;
    }

    a_rotation[0].identity();
    a_rotation[1].identity();

    cMatrix3d T;
    T.set(
        0, 0, 1,
        1, 0, 0,
        0, 1, 0
    );

    cMatrix3d Tinv = cTranspose(T); // 直交行列なら inverse = transpose

    for (uint32_t h = 0; h < _lastTrackingEvent.nHands && h < 2; h++)
    {
        LEAP_HAND* hand = &_lastTrackingEvent.pHands[h];
        int handIndex = (hand->type == eLeapHandType_Right) ? 0 : 1;

        LEAP_QUATERNION q = hand->palm.orientation;

        double n = sqrt(q.x*q.x + q.y*q.y + q.z*q.z + q.w*q.w);
        if (n < 1e-8)
        {
            a_rotation[handIndex].identity();
            continue;
        }

        double x = q.x / n;
        double y = q.y / n;
        double z = q.z / n;
        double w = q.w / n;

        cMatrix3d Rl;
        Rl.set(
            1 - 2*(y*y + z*z),   2*(x*y - z*w),       2*(x*z + y*w),
            2*(x*y + z*w),       1 - 2*(x*x + z*z),   2*(y*z - x*w),
            2*(x*z - y*w),       2*(y*z + x*w),       1 - 2*(x*x + y*y)
        );

        a_rotation[handIndex] = T * Rl * Tinv;
    }

    return true;
}


//==========================================================================
/*!
    Return the hand pinching motion angle from the tracking data retrieved by the last
    \ref tdLeapUpdate() call.

    \fn       bool __FNCALL tdLeapGetGripperAngleRad(double a_angle[2])

    \param a_angle   Returned hand pinching angle.

    \return Return __true__ on success, __false__ otherwise.
*/
//==========================================================================
bool __FNCALL tdLeapGetGripperAngleRad(double a_angle[2])
{
    // gripper maximum opening angle (in deg)
    const double OPEN_ANGLE = 30.0;

    // check if device is physically available
    if (!_connection || !_isConnected || !_hasTracking)
    {
        return (false);
    }

    // Initialize angles to fully open
    a_angle[0] = OPEN_ANGLE * DEG_TO_RAD;
    a_angle[1] = OPEN_ANGLE * DEG_TO_RAD;

    // Process hands
    for (uint32_t h = 0; h < _lastTrackingEvent.nHands && h < 2; h++)
    {
        LEAP_HAND* hand = &_lastTrackingEvent.pHands[h];
        
        // Determine hand index (0 for right, 1 for left)
        int handIndex = (hand->type == eLeapHandType_Right) ? 0 : 1;
        
        // Get pinch strength (0.0 = not pinching, 1.0 = full pinch)
        a_angle[handIndex] = (1.0 - hand->pinch_strength) * OPEN_ANGLE * DEG_TO_RAD;
    }

    return (true);
}


//==========================================================================
/*!
    Return the hand open/close status from the tracking data retrieved by the last
    \ref tdLeapUpdate() call.

    \fn       bool __FNCALL tdLeapGetUserSwitches(unsigned int a_userSwitches[2])

    \param a_userSwitches   Returned hand open/close status.

    \return Return __true__ on success, __false__ otherwise.
*/
//==========================================================================
bool __FNCALL tdLeapGetUserSwitches(unsigned int a_userSwitches[2])
{
    // check if device is physically available
    if (!_connection || !_isConnected || !_hasTracking)
    {
        return (false);
    }

    // Initialize switches to off
    a_userSwitches[0] = 0x00;
    a_userSwitches[1] = 0x00;

    // Process hands
    for (uint32_t h = 0; h < _lastTrackingEvent.nHands && h < 2; h++)
    {
        LEAP_HAND* hand = &_lastTrackingEvent.pHands[h];
        
        // Determine hand index (0 for right, 1 for left)
        int handIndex = (hand->type == eLeapHandType_Right) ? 0 : 1;
        
        // Get grab strength (0.0 = open hand, 1.0 = fist)
        if (hand->grab_strength > 0.75)
        {
            a_userSwitches[handIndex] = 0x01;
        }
    }

    return (true);
}


//==========================================================================
/*!
    Returns the latest tracking event from a given Leap Motion device.

    \fn       bool __FNCALL tdLeapGetFrame(void* &a_frame)

    \param a_frame  An (unallocated) pointer that will point to a new LEAP_TRACKING_EVENT on the heap.
                    Cast to LEAP_TRACKING_EVENT* to access all the LeapC functionality.

    \return Return __true__ on success, __false__ otherwise.

    \note
    It is the responsibility of the caller to deallocate (delete)
    the tracking event pointer afterwards.
*/
//==========================================================================
bool __FNCALL tdLeapGetFrame(void* &a_frame)
{
    // check if device is physically available
    if (!_connection || !_isConnected || !_hasTracking)
    {
        a_frame = NULL;
        return (false);
    }

    // Allocate and copy the tracking event
    LEAP_TRACKING_EVENT* event = new LEAP_TRACKING_EVENT;
    memcpy(event, &_lastTrackingEvent, sizeof(LEAP_TRACKING_EVENT));
    
    a_frame = (void*)event;
    return (true);
}

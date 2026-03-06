//==============================================================================
/*
    Software License Agreement (BSD License)
    Copyright (c) 2019-2022, AMBF
    (https://github.com/WPI-AIM/ambf)

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

    * Neither the name of authors nor the names of its contributors may
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

    \author    <amunawar@jhu.edu>
    \author    Adnan Munawar
*/
//==============================================================================
#ifndef FOOTPEDAL_H
#define FOOTPEDAL_H

#include "joystick.h"

// Original joystick button mappings
enum class FootPedalJoystickMap{
    CAM_CLUTCH = 0,      // Button 0 on joystick
    CHANGE_BURR_SIZE = 1, // Button 1 on joystick  
    DEVICE_CLUTCH = 2     // Button 2 on joystick
};

// Keyboard button mappings (for FootSwitch) - using high key codes to avoid conflicts
enum class FootPedalKeyboardMap{
    BURR_STATE = 3,        // Custom high key code (mapped from B key)
    CHANGE_BURR_SIZE = 4,  // Custom high key code (mapped from C key)
    CAM_CLUTCH = 5      // Custom high key code (for future use)
};

class FootPedal: public JoyStick{
public:
    bool isDrillOn();

    bool isChangeBurrSizePressed();

    bool isCamClutchPressed();

    bool isDeviceClutchPressed();

     // Getters that work for both joystick and keyboard
    bool getCamClutchState();
    bool getChangeBurrSizeState(); 
    bool getDeviceClutchState();

private:
    bool m_burrChangeBtnPrevState = false;
    bool m_drillBtnPrevState = false;
    
    // Current states for both joystick and keyboard mappings
    bool m_camClutchState = false;
    bool m_changeBurrSizeState = false;
    bool m_deviceClutchState = false;
};

#endif

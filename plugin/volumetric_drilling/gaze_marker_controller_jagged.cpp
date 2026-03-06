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
    
    Modified by: Jonathan Wang
    This "jagged" gaze marker jumps across the screen instead of
    gliding smoothly!
*/
//==============================================================================

#include "gaze_marker_controller_jagged.h"
#include <boost/program_options.hpp>

GazeMarkerController::GazeMarkerController(){
    m_gazeMarker = nullptr;
    m_manualControl = false;
}

int GazeMarkerController::init(afWorldPtr a_worldPtr, CameraPanelManager* a_panelManager){
    m_gazeMarker = a_worldPtr->getRigidBody("GazeMarker");
    if (!m_gazeMarker){
        cerr << "ERROR! GAZE MARKER RIGID BODY NOT FOUND. CAN'T PERFORM GAZE CALIBRATION MOTION" << endl;
        return -1;
    }

    m_gazeMarker->scaleSceneObjects(0.5);
    m_mainCamera = a_worldPtr->getCamera("main_camera");
    m_panelManager = a_panelManager;

    m_T_c_w = m_mainCamera->getLocalTransform();
    m_T_m_c = cTransform(cVector3d(-5., 0., 0.), cMatrix3d());

    m_T_m_w = m_T_c_w * m_T_m_c;
    m_gazeMarker->setLocalTransform(m_T_m_w);

    m_textShowDuration = 5.0;
    m_time = m_textShowDuration;

    m_posIdx = 10;
    m_posDur = 5.0; // try to double the time
    m_posStartTime = 0.;

    m_gridWidth = 0.25;
    m_gridHeight = 0.25;
    m_gridCenter = 0.0;
    m_cornerOffset = 0.05;

    // Reduce size of 3x3 calibration grid
    double scaleFactor = 0.75;
    m_gridWidth *= scaleFactor;
    m_gridHeight *= scaleFactor;
    m_cornerOffset *= scaleFactor;

    m_P_m_c_list = {
        cVector3d(-5.,  m_gridCenter, m_gridCenter),
        cVector3d(-5., -m_gridWidth + m_cornerOffset,  m_gridHeight - m_cornerOffset),
        cVector3d(-5.,  m_gridWidth - m_cornerOffset, -m_gridHeight + m_cornerOffset),
        cVector3d(-5.,  m_gridWidth - m_cornerOffset,  m_gridHeight - m_cornerOffset),
        cVector3d(-5., -m_gridWidth + m_cornerOffset, -m_gridHeight + m_cornerOffset),
        cVector3d(-5.,  m_gridWidth,  m_gridCenter),
        cVector3d(-5., -m_gridWidth, -m_gridCenter),
        cVector3d(-5.,  m_gridCenter, m_gridHeight),
        cVector3d(-5.,  m_gridCenter,-m_gridHeight),
    };

    initializeLabels();

    // restart();

    return 1;
}

void GazeMarkerController::initializeLabels(){
    cFontPtr font = NEW_CFONTCALIBRI36();
    m_gazeNotificationLabel = new cLabel(font);
    m_gazeNotificationLabel->m_fontColor.setBlack();
    m_textStr = "PLEASE FOCUS ON THE CIRCULAR MARKER \n\n"
                "             SHOWING MARKER IN : ";
    m_gazeNotificationLabel->setText(m_textStr);

    m_gazeNotificationLabel->setCornerRadius(10, 10, 10, 10);
    m_gazeNotificationLabel->setColor(cColorf(1., 1., 0.2));
    m_gazeNotificationLabel->setShowPanel(true);
    m_gazeNotificationLabel->setTransparencyLevel(0.8);
    m_gazeNotificationLabel->setShowEnabled(false);

    m_panelManager->addPanel(m_gazeNotificationLabel, 0.5, 0.5, PanelReferenceOrigin::CENTER, PanelReferenceType::NORMALIZED);
}

void GazeMarkerController::update(double dt){
    // If in manual control mode, don't auto-hide the marker
    if (m_manualControl){
        return;  // Skip calibration logic when in manual mode
    }

    if (m_posIdx >= (m_P_m_c_list.size()+1) || m_gazeMarker == nullptr){
        hide(true);
        return;
    }

    if (m_time == 0.){
        hide(false);
        cMatrix3d rot;
        rot.identity();
        cTransform trans(cVector3d(-100, 0, 0), rot);
        m_gazeMarker->setLocalTransform(trans); // Set the marker to be way behind the vol
    }

    m_time += dt;

    if (m_time <= m_textShowDuration){
        string time_str = to_string(int(ceil(m_textShowDuration - m_time)));
        m_panelManager->setText(m_gazeNotificationLabel, m_textStr + time_str);
        return;
    }

    m_panelManager->setVisible(m_gazeNotificationLabel, false);

    double elapsed_time = m_time - m_textShowDuration - m_posStartTime;

    if ((m_time - m_textShowDuration) <= m_posDur && m_posIdx == 0){
        // m_T_m_c = cTransform(m_P_m_c_list[m_posIdx], cMatrix3d());
        m_T_m_c = cTransform(m_P_m_c_list[m_posIdx], m_mainCamera->getLocalRot());
        m_T_m_w = m_T_c_w * m_T_m_c;
        m_gazeMarker->setLocalTransform(m_T_m_w);
        
        m_posIdx++;
        return;
    }

    if (elapsed_time >= m_posDur){
        // m_T_m_c = cTransform(m_P_m_c_list[m_posIdx], cMatrix3d());
        m_T_m_c = cTransform(m_P_m_c_list[m_posIdx], m_mainCamera->getLocalRot());
        m_T_m_w = m_T_c_w * m_T_m_c;
        m_gazeMarker->setLocalTransform(m_T_m_w);

        m_posIdx++;
        m_posStartTime = m_time - m_textShowDuration;
    }

}

void GazeMarkerController::hide(bool val){
    if (m_gazeMarker){
        m_gazeMarker->getVisualObject()->setShowEnabled(!val);
        m_panelManager->setVisible(m_gazeNotificationLabel, !val);
    }
}

void GazeMarkerController::restart(){
    if (m_gazeMarker){
        cerr << "Restarting Gaze Marker Motion" << endl;
        // Disable manual control when calibration starts
        m_manualControl = false;

        m_time = 0.;
        m_gazeMarker->reset();
        m_T_c_w = m_mainCamera->getLocalTransform();
        m_T_m_c = cTransform(cVector3d(-5., 0., 0.), m_mainCamera->getLocalRot());
        m_T_m_w = m_T_c_w * m_T_m_c;;

        m_posIdx = 0;
        m_posStartTime = 0.;

    }
}

void GazeMarkerController::showMarkerAtCameraCenter(){
    if (m_gazeMarker && m_mainCamera){
        // Enable manual control mode
        m_manualControl = true;

        // Get current camera transform
        m_T_c_w = m_mainCamera->getLocalTransform();
        
        // Position marker in center of camera view (5 units in front)
        // m_T_m_c = cTransform(cVector3d(-5., 0., 0.), cMatrix3d());
        m_T_m_c = cTransform(cVector3d(-5., 0., 0.), m_mainCamera->getLocalRot());
        
        // Calculate marker world transform
        m_T_m_w = m_T_c_w * m_T_m_c;
        m_gazeMarker->setLocalTransform(m_T_m_w);
        
        // Make marker visible
        m_gazeMarker->getVisualObject()->setShowEnabled(true);
        
        cerr << "Marker shown at camera center (manual control enabled)" << endl;
    }
}

void GazeMarkerController::hideMarker(){
    if (m_gazeMarker){
        // Enable manual control mode
        m_manualControl = true;

        m_gazeMarker->getVisualObject()->setShowEnabled(false);
        cerr << "Marker hidden" << endl;
    }
}

void GazeMarkerController::toggleMarker(){
    if (m_gazeMarker){
        bool isVisible = m_gazeMarker->getVisualObject()->getShowEnabled();
        
        if (isVisible){
            hideMarker();
        } else {
            showMarkerAtCameraCenter();
        }
    }
}

bool GazeMarkerController::isMarkerVisible(){
    if (m_gazeMarker){
        return m_gazeMarker->getVisualObject()->getShowEnabled();
    }
    return false;
}
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
    This "smooth" gaze marker glides smoothly across the screen instead
    of jumping positions.
*/
//==============================================================================
#include "gaze_marker_controller_smooth.h"
#include <boost/program_options.hpp>

GazeMarkerController::GazeMarkerController() {
    m_gazeMarker = nullptr;
    m_transitioning = false;
    m_transitionDuration = 2.0; // Smooth transition over 2 seconds
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
    // m_T_m_c = cTransform(cVector3d(-5., 0., 0.), cMatrix3d());
    cVector3d initialPos(-5., 0., 0.);
    cMatrix3d initialRot = computeRotationToCamera(initialPos);
    m_T_m_c = cTransform(initialPos, initialRot);

    m_T_m_w = m_T_c_w * m_T_m_c;
    m_gazeMarker->setLocalTransform(m_T_m_w);

    m_textShowDuration = 5.0;
    m_time = m_textShowDuration;

    m_posIdx = 10;
    m_posDur = 7.0;
    m_posStartTime = 0.;

    m_gridWidth = 0.25;
    m_gridHeight = 0.25;
    m_gridCenter = 0.0;
    m_cornerOffset = 0.05;

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

void GazeMarkerController::update(double dt) {
    if (m_posIdx >= m_P_m_c_list.size() || m_gazeMarker == nullptr) {
        hide(true);
        return;
    }

    // Hide the marker during the initial countdown
    if (m_time == 0.){
        hide(false); // Hide the marker instead of moving it off-screen
        cMatrix3d rot;
        rot.identity();
        cTransform trans(cVector3d(-100, 0, 0), rot);
        m_gazeMarker->setLocalTransform(trans); // Set the marker to be way behind the vol
    }
    
    m_time += dt;

    // Show countdown text
    if (m_time <= m_textShowDuration) {
        string time_str = to_string(int(ceil(m_textShowDuration - m_time)));
        m_panelManager->setText(m_gazeNotificationLabel, m_textStr + time_str);
        return;
    }

    // Hide text after countdown
    m_panelManager->setVisible(m_gazeNotificationLabel, false);

    if ((m_time - m_textShowDuration) <= m_posDur && m_posIdx == 0){
        cVector3d pos = m_P_m_c_list[m_posIdx];
        cMatrix3d rot = computeRotationToCamera(pos);
        m_T_m_c = cTransform(pos, rot);
        m_T_m_w = m_T_c_w * m_T_m_c;
        m_gazeMarker->setLocalTransform(m_T_m_w);
        return;
    }

    if (m_transitioning) {
        // Handle smooth transition between positions
        handleTransition();
    } else {
        // Check if hold time at current position is complete
        double elapsedHold = m_time - m_textShowDuration - m_posStartTime;
        if (elapsedHold >= m_posDur) {
            if (m_posIdx < m_P_m_c_list.size() - 1) {
                startTransition(); // Start gliding to next position
            } else {
                hide(true); // End of sequence
            }
        }
    }
}

void GazeMarkerController::startTransition() {
    m_transitioning = true;
    m_transitionStartTime = m_time;
    m_startTransitionPos = m_P_m_c_list[m_posIdx];
    m_endTransitionPos = m_P_m_c_list[m_posIdx + 1];
}    

void GazeMarkerController::handleTransition() {
    double transitionElapsed = m_time - m_transitionStartTime;
    double alpha = transitionElapsed / m_transitionDuration;

    if (alpha >= 1.0) {
        // Transition complete: hold at the new position
        m_transitioning = false;
        m_posIdx++;
        m_posStartTime = m_time - m_textShowDuration; // Reset hold timer
        
        // Set final position with correct rotation
        cMatrix3d endRot = computeRotationToCamera(m_endTransitionPos);
        m_T_m_c = cTransform(m_endTransitionPos, endRot);
        m_T_m_w = m_T_c_w * m_T_m_c;
        m_gazeMarker->setLocalTransform(m_T_m_w);
    } else {
        // Linear interpolation between start and end positions
        cVector3d newPos = m_startTransitionPos + (m_endTransitionPos - m_startTransitionPos) * alpha;
        // Calculate proper rotation for current position
        cMatrix3d newRot = computeRotationToCamera(newPos); 
        m_T_m_c = cTransform(newPos, newRot);
        m_T_m_w = m_T_c_w * m_T_m_c;
        m_gazeMarker->setLocalTransform(m_T_m_w);
    }
}

void GazeMarkerController::hide(bool val){
    if (m_gazeMarker){
        m_gazeMarker->getVisualObject()->setShowEnabled(!val);
        m_panelManager->setVisible(m_gazeNotificationLabel, !val);
    }
}

void GazeMarkerController::restart() {
    if (m_gazeMarker) {
        cerr << "Restarting Gaze Marker Motion" << endl;
        m_time = 0.;
        m_gazeMarker->reset();
        m_T_c_w = m_mainCamera->getLocalTransform();
        m_posIdx = 0;
        m_posStartTime = 0.;
        m_transitioning = false;
        
        // Reset to first position
        // m_T_m_c = cTransform(m_P_m_c_list[0], cMatrix3d());
        cVector3d startPos = m_P_m_c_list[0];
        cMatrix3d startRot = computeRotationToCamera(startPos);
        m_T_m_c = cTransform(startPos, startRot);
        m_T_m_w = m_T_c_w * m_T_m_c;
        m_gazeMarker->setLocalTransform(m_T_m_w);
    }
}

cMatrix3d GazeMarkerController::computeRotationToCamera(const cVector3d& position) {
    // Camera is at origin in camera space, direction TO camera is position vector
    cVector3d toCamera = position; 
    if (toCamera.length() < 0.001) return cMatrix3d();

    toCamera.normalize();
    cVector3d up(0, 1, 0); // Use camera's up axis

    // Compute right vector
    cVector3d right = cCross(up, toCamera);
    if (right.length() < 0.001) {
        // Handle edge case (looking directly up/down)
        up.set(0, 0, 1);
        right = cCross(up, toCamera);
    }
    right.normalize();

    // Compute corrected up vector
    cVector3d newUp = cCross(toCamera, right);
    newUp.normalize();

    // Build rotation matrix (Z-axis points TOWARDS camera)
    cMatrix3d rot;
    rot.setCol0(right);     // X-axis
    rot.setCol1(newUp);     // Y-axis
    rot.setCol2(-toCamera); // Z-axis (negative direction to face camera)
    return rot;
}
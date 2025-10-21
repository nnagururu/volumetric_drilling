//==============================================================================
/*
Script Name: Video Recording Controller
Author: Jonathan Wang
Date Created: 2024-12-03
Last Modified: 2025-01-15
Version: 1.1
Updates: generate world_timestamps.npy

Description:
    This script helps control the recording of the simulator 'world' view upon clicking
    the 'Record Study' button on the Drilling Simulator GUI. It stores the video in the
    same directory that contains other recorded metrics (i.e. removed voxels, gaze capture).
    Adapted from Adnan Munawar's script https://github.com/adnanmunawar/ambf_video_recording.

Usage:
    Called within the volumetric_drilling.cpp and initalized within the volumetric_drilling.h files.
*/
//==============================================================================

#include "video_recording_controller.h"
#include <boost/program_options.hpp>
#include <filesystem>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <cnpy.h>
#include <fstream>

using namespace std;
namespace fs = std::filesystem;

//------------------------------------------------------------------------------
// DECLARED FUNCTIONS
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
namespace p_opt = boost::program_options;

string g_current_filepath;

VideoRecordingController::VideoRecordingController()
{

}

//------------------------------------------------------------------------------
// Function to create a new subdirectory inside "Simulator_Recordings" directory
//------------------------------------------------------------------------------
string createNewDirectory(const string& baseDir, const string& startingDir) {
    // Create the overarching "Simulator_Recordings" directory if it does not exist
    string baseDirectory = startingDir + "/" + baseDir;
    if (!fs::exists(baseDirectory)) {
        if (!fs::create_directory(baseDirectory)) {
            cerr << "Failed to create base directory: " << baseDirectory << endl;
            throw runtime_error("Base directory creation failed");
        }
    }
    // Get current time for a unique directory name
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t_now), "%Y%m%d_%H%M%S");
    string newDir = baseDirectory + "/" + ss.str();
    // Create the directory inside the base directory
    if (!fs::create_directories(newDir)) {
        cerr << "Failed to create directory: " << newDir << endl;
        throw runtime_error("Directory creation failed");
    }
    return newDir;
}

int VideoRecordingController::init(const afWorldPtr a_afWorld, const string& startingDir) {
    cerr << "INFO! Initializing video recorder plugin variables...\n";

    m_worldPtr = a_afWorld;
    m_camera = m_worldPtr->getCameras()[0];

    // Copy resolution from the camera
    m_width = m_camera->m_width;
    m_height = m_camera->m_height;

    // Setup frame buffer
    m_frameBuffer = cFrameBuffer::create();
    m_frameBuffer->setup(m_camera->getInternalCamera(), m_width, m_height, true, true);

    m_image = cImage::create();

    // Set the directory to save recordings
    // m_saveDirectory = createNewDirectory("Simulator_Recordings", startingDir);
    m_saveDirectory = startingDir; // save inside the pupil data folder of 000
    cerr << "Recording save directory: " << m_saveDirectory << endl;

    // Calculate and save camera intrinsics
    calculateCameraIntrinsics();
    m_intrinsics_filename = m_saveDirectory + "/" + "world.intrinsics";  // Changed to .intrinsics
    saveIntrinsicsToFile(m_intrinsics_filename);
    
    cerr << "INFO! Variables initialized successfully.\n";
    return 1;
}

int VideoRecordingController::start_recording(const double world_timestamp) {
    if (!m_camera || m_saveDirectory.empty()) {
        cerr << "ERROR! Camera or save directory is not initialized. Call init first.\n";
        return -1;
    }

    // Generate video file path
    time_t now = time(0);
    string size_str = to_string(m_width) + "x" + to_string(m_height);
    m_video_filename = m_saveDirectory + "/" + "world.mp4";
    m_timestamps_filename = m_saveDirectory + "/" + "world_timestamps.npy";

    // FFmpeg settings
    string cmd = "ffmpeg";
    cmd += " -r 60";
    cmd += " -f rawvideo";
    cmd += " -pix_fmt rgba";
    cmd += " -s " + size_str;
    cmd += " -i -";
    cmd += " -threads 0";
    cmd += " -preset fast"; 
    cmd += " -y";
    cmd += " -pix_fmt yuv420p";
    cmd += " -crf 21"; // Major parameter to tweak video compression and output size
    // cmd += " -vf vflip";
    cmd += " -vf \"crop=iw/2:ih:iw/4:0,vflip\"";  // FOR CROPPING MIDDLE 50% OF SCREEN
    cmd += " " + m_video_filename;
    
    // Start FFmpeg process for recording video
    try {
        m_ffmpeg = popen(cmd.c_str(), "w");
        cerr << "INFO! Recording started: " << m_video_filename << "\n";
    } catch (exception& e) {
        cerr << "ERROR! Failed to start recording: " << e.what() << "\n";
        return -1;
    }

    // Prepare timestamp storage
    m_recorded_timestamps.clear();
    m_recorded_timestamps.push_back(world_timestamp); // Record the first timestamp
    return 1;
}

void VideoRecordingController::save_timestamps_to_npy(const string& filename, const vector<double>& timestamps) {
    try {
        // Convert the vector to a raw pointer for saving
        const double* data_ptr = timestamps.data();
        size_t size = timestamps.size();

        // Save the data using cnpy
        cnpy::npy_save(filename, data_ptr, {size}, "w");

        cerr << "INFO! Timestamps saved to: " << filename << "\n";
    } catch (const std::exception& e) {
        throw runtime_error("Failed to save timestamps to .npy file: " + string(e.what()));
    }
}

/***
int VideoRecordingController::start_recording() {
    if (!m_camera || m_saveDirectory.empty()) {
        cerr << "ERROR! Camera or save directory is not initialized. Call init first.\n";
        return -1;
    }

    // Generate video file path
    time_t now = time(0);
    string size_str = to_string(m_width) + "x" + to_string(m_height);
    // m_video_filename = m_saveDirectory + "/" + m_camera->getName() + "_" + to_string(int(m_worldPtr->getSystemTime())) + ".mp4";
    m_video_filename = m_saveDirectory + "/" + "world.mp4";
    // ffmpeg settings
    string cmd = "ffmpeg";
    cmd += " -r 60";
    cmd += " -f rawvideo";
    cmd += " -pix_fmt rgba";
    cmd += " -s " + size_str;
    cmd += " -i -";
    cmd += " -threads 0";
    cmd += " -preset fast";
    cmd += " -y";
    cmd += " -pix_fmt yuv420p";
    cmd += " -crf 21"; // Major parameter to tweak video compression and output size
    cmd += " -vf vflip";
    cmd += " " + m_video_filename;

    try {
        m_ffmpeg = popen(cmd.c_str(), "w");
        cerr << "INFO! Recording started: " << m_video_filename << "\n";
    } catch (exception& e) {
        cerr << "ERROR! Failed to start recording: " << e.what() << "\n";
        return -1;
    }

    return 1;
}
***/

void VideoRecordingController::keyboardUpdate(GLFWwindow *a_window, int a_key, int a_scancode, int a_action, int a_mods)
{

}

void VideoRecordingController::mouseBtnsUpdate(GLFWwindow *a_window, int a_button, int a_action, int a_modes)
{

}

void VideoRecordingController::mousePosUpdate(GLFWwindow *a_window, double x_pos, double y_pos) {}

void VideoRecordingController::mouseScrollUpdate(GLFWwindow *a_window, double x_pos, double y_pos)
{

}

void VideoRecordingController::update(const double current_timestamp)
{
    try {
        m_frameBuffer->renderView();
        m_frameBuffer->copyImageBuffer(m_image);

        // Write the frame data to FFmpeg
        if (m_ffmpeg) {
            fwrite(m_image->getData(), m_image->getBytesPerPixel() * m_width * m_height, 1, m_ffmpeg);
            m_recorded_timestamps.push_back(current_timestamp); // Record the timestamp
        } else {
            cerr << "ERROR! FFmpeg process not initialized. Call start_recording first.\n";
            return;
        }

        // Save the first frame as an image (only once)
        static bool save_first_frame_img = true;
        if (save_first_frame_img) {
            string first_frame_filename = m_camera->getName() + ".png";
            m_image->saveToFile(first_frame_filename);
            cerr << "INFO! First frame saved to: " << first_frame_filename << "\n";
            save_first_frame_img = false;
        }
    } catch (const exception& e) {
        cerr << "ERROR! Exception in update: " << e.what() << "\n";
    }
}

void VideoRecordingController::physicsUpdate(double dt)
{

}

void VideoRecordingController::reset()
{

}


bool VideoRecordingController::close()
{
    if (m_ffmpeg){
        pclose(m_ffmpeg);
        m_ffmpeg = nullptr;
        // Save timestamps to file
        try {
            save_timestamps_to_npy(m_timestamps_filename, m_recorded_timestamps);
        } catch (const exception& e) {
            cerr << "ERROR! Failed to save timestamps: " << e.what() << "\n";
            return false;
        }

        cerr << "INFO! Recording completed and resources released.\n";
    }
    return true;
}


void VideoRecordingController::calculateCameraIntrinsics() {
    // Get camera parameters from your AMBF camera
    double fov_angle = m_camera->getFieldViewAngle(); // radians
    double width = m_camera->m_width;
    double height = m_camera->m_height;
    
    // Calculate focal length from field of view
    // f = (width / 2) / tan(fov_angle / 2)
    double focal_length_x = (width / 2.0) / tan(fov_angle / 2.0);
    double focal_length_y = (height / 2.0) / tan(fov_angle / 2.0);
    
    // For most cameras, we assume fx ≈ fy
    double fx = focal_length_x;
    double fy = focal_length_y;
    
    // Principal point (usually center of image)
    double cx = width / 2.0;
    double cy = height / 2.0;
    
    // Create camera intrinsics matrix
    // [fx  0  cx]
    // [0  fy  cy]
    // [0   0   1]
    m_camera_intrinsics << fx,  0,  cx,
                           0,  fy,  cy,
                           0,   0,   1;
    
    cerr << "Camera Intrinsics Calculated:\n";
    cerr << "FOV: " << fov_angle << " rad (" << fov_angle * 180.0/M_PI << " deg)\n";
    cerr << "Resolution: " << width << " x " << height << "\n";
    cerr << "Focal Length (fx, fy): " << fx << ", " << fy << "\n";
    cerr << "Principal Point (cx, cy): " << cx << ", " << cy << "\n";
}

void VideoRecordingController::saveIntrinsicsToFile(const string& filename) {
    try {
        ofstream file(filename);
        if (!file.is_open()) {
            throw runtime_error("Cannot open file for writing: " + filename);
        }
        
        // Save in world.intrinsics format
        // Format typically used by computer vision applications
        file << "intrinsic" << endl;
        file << m_camera_intrinsics(0,0) << " " << m_camera_intrinsics(0,1) << " " << m_camera_intrinsics(0,2) << endl;
        file << m_camera_intrinsics(1,0) << " " << m_camera_intrinsics(1,1) << " " << m_camera_intrinsics(1,2) << endl;
        file << m_camera_intrinsics(2,0) << " " << m_camera_intrinsics(2,1) << " " << m_camera_intrinsics(2,2) << endl;
        
        file.close();
        cerr << "INFO! Camera intrinsics saved to: " << filename << "\n";
        
        // Also save as numpy array for convenience
        vector<double> intrinsics_data = {
            m_camera_intrinsics(0,0), m_camera_intrinsics(0,1), m_camera_intrinsics(0,2),
            m_camera_intrinsics(1,0), m_camera_intrinsics(1,1), m_camera_intrinsics(1,2),
            m_camera_intrinsics(2,0), m_camera_intrinsics(2,1), m_camera_intrinsics(2,2)
        };
        cnpy::npy_save(filename + ".npy", intrinsics_data.data(), {3, 3}, "w");
        
    } catch (const exception& e) {
        throw runtime_error("Failed to save camera intrinsics: " + string(e.what()));
    }
}
#include "Arena.h"

#include <stdlib.h>
#include <opencv2/imgproc.hpp>
#include <QMutexLocker>

const float PI = 3.141592653589;
const float TARGET_DIAMETER = 0.18;

Arena::Arena()
{
    mOriginPx[0] = 500;
    mOriginPx[1] = 500;
    mXAxisPx[0] = 600;
    mXAxisPx[1] = 600;
    mObstacles[0] = {2.0, 2.0, 1.0, 1.0};
    mObstacles[1] = {3.0, 1.5, 0.8, 0.2};
    mObstacles[2] = {1.5, 3.2, 1.2, 1.4};
//    mPpm = 1;
    mWidthM = 4;
    mHeightM = 2;
}

// Converts arena coordinates into pixel coordinates
cv::Point Arena::cameraCoordinate(float x, float y) {
    QMutexLocker lock(&mArenaMutex);
    // Convert coordinates to pixels
    float A = x * mPpm;
    float B = y * mPpm;
    
    // Transform into the arena
    float fx = A * cos(mTheta) - B * sin(mTheta);
    float fy = A * sin(mTheta) + B * cos(mTheta);
    
    return cv::Point(mOriginPx[0] + fx, mOriginPx[1] - fy);
}

// Draws the starting position, obstacles, and destination
void Arena::draw(cv::Mat& image) {
    mObstaclesMutex.lock();
    if (mDrawObstacles) {
        for (int i = 0; i < 3; i++) {
            drawRectangle(image, mObstacles[i].x, mObstacles[i].y, mObstacles[i].width, mObstacles[i].height);
        }
    }
    mObstaclesMutex.unlock();
    mDestinationMutex.lock();
    if (mDrawDestination) {
        drawCircle(image, mTargetLocation.x, mTargetLocation.y, TARGET_DIAMETER / 2);
    }
    mDestinationMutex.unlock();
    mCustomMutex.lock();
    if(mDrawCustom) {
        drawCircle(image, mCustomCoordinate.x, mCustomCoordinate.y, 0.09);
    }
    mCustomMutex.unlock();
    drawRectangle(image, mStartingLocation.x - 0.175, mStartingLocation.y + 0.175, 0.35, 0.35);

    Position pt1(mStartingLocation.x - 0.1 * cos(mStartingLocation.theta),
                    mStartingLocation.y - 0.1 * sin(mStartingLocation.theta), 0);
    Position pt2(mStartingLocation.x + 0.1 * cos(mStartingLocation.theta),
                 mStartingLocation.y + 0.1 * sin(mStartingLocation.theta), 0);

    cv::arrowedLine(image, cameraCoordinate(pt1.x, pt1.y),
                    cameraCoordinate(pt2.x, pt2.y),
                    cv::Scalar(0, 255, 0),
                    3,
                    8,
                    0,
                    0.3);
}

// Draws a circle at an arena X,Y location
void Arena::drawCircle(cv::Mat& image, float x, float y, float radius) {
    cv::ellipse(image,          // Image to draw on
        cameraCoordinate(x, y), // Coordinates of center
        cv::Size(radius * mPpm, radius * mPpm),// Radius
        0,                      // Rotation
        0,                      // Arc start degree
        360,                    // Arc end degree
        cv::Scalar(255, 200, 0),  // Color (BGR)
        2                       // Line thickness
    );
}

// Draws a rectangle in the arena
// @params x, y are arena coordinate of upper left corner of rectangle
// @params width, height are in meters
void Arena::drawRectangle(cv::Mat& image, float x, float y, float width, float height) {
    std::vector<cv::Point> points(4);

    points[0] = cameraCoordinate(x, y);
    points[1] = cameraCoordinate(x + width, y);
    points[2] = cameraCoordinate(x + width, y - height);
    points[3] = cameraCoordinate(x, y - height);

    cv::Mat contours = cv::Mat(points);
    cv::polylines(image,
        contours,
        true,
        cv::Scalar(0, 255, 0),
        2
    );
}

bool Arena::getPosition(int markerId, Marker& marker) {
    mMarkersMutex.lock();
    if(mMarkers.contains(markerId)){
        marker = mMarkers.value(markerId, Marker(markerId));
        mMarkersMutex.unlock();
        return true;
    }
    mMarkersMutex.unlock();
    return false;
}

// Translates and stores a set of detected markers
void Arena::processMarkers(cv::Mat& image, std::vector<aruco::Marker>& markers) {
    mMarkersMutex.lock();
    mMarkers.clear();

    foreach (aruco::Marker marker, markers) {
        marker.draw(image, cv::Scalar(0, 0, 255), 2);

        if (marker.id == 0) {
            mArenaMutex.lock();
            mOriginPx[0] = marker[0].x;
            mOriginPx[1] = marker[0].y;
            mArenaMutex.unlock();
        } else if (marker.id == 1) {
            mArenaMutex.lock();
            mXAxisPx[0] = marker[0].x;
            mXAxisPx[1] = marker[0].y;
            mArenaMutex.unlock();
        } else {
            mMarkers.insert(marker.id, translate(marker));

            // Draw an arrow indicating the orientation
            cv::arrowedLine(image,
                cv::Point(marker[0].x, marker[0].y),
                cv::Point(marker[1].x, marker[1].y),
                cv::Scalar(0, 255, 0),
                3,
                8,
                0,
                0.5
            );
        }
    }

    mMarkersMutex.unlock();

    mArenaMutex.lock();
    // Calculate pixels per meter for the frame
    mPpm = sqrt((mXAxisPx[0] - mOriginPx[0])*(mXAxisPx[0] - mOriginPx[0]) +
        (mXAxisPx[1] - mOriginPx[1]) * (mXAxisPx[1] - mOriginPx[1])) / mWidthM;
    // Calculate the tilt of the arena in this frame
    mTheta = -atan2(mXAxisPx[1] - mOriginPx[1], mXAxisPx[0] - mOriginPx[0]);
    mArenaMutex.unlock();
}

// Randomizes mission variables
void Arena::randomize() {
    static const float quadrantBounds[4][4] = {
        // Min x, Max x, Min y, Max y
        {1.4, 2.25, 1.0, 1.8},
        {1.4, 2.25, 0.2, 1.0},
        {2.25, 3.8, 0.2, 1.0},
        {2.25, 3.8, 1.0, 1.8}
    };
    
    // Generate a random starting position
    mStartingLocation.x = 0.35;
    mStartingLocation.y = 0.4 + (rand() % 5) * 0.3;
    mStartingLocation.theta = (rand() % 4) * PI / 2 - PI;

    // Generate random positions and orientations for obstacles
    mDestinationMutex.lock();
    mObstaclesMutex.lock();
    int largeObstacleQuadrant = rand() % 3;
    for (int i = 0; i < 3; i++) {
        // Reassign sizes based on randomization
        float d1 = i == largeObstacleQuadrant ? 0.41 : 0.32;
        float d2 = i == largeObstacleQuadrant ? 0.23 : 0.13;

        // Random orientation
        if (rand() % 2) {
            mObstacles[i].width = d1;
            mObstacles[i].height = d2;
        } else {
            mObstacles[i].width = d2;
            mObstacles[i].height = d1;
        }
    }

    for (int i = 0; i < 2; i++) {
        // Randomize locations
        mObstacles[i].x =
            (rand() % 100) *
            (quadrantBounds[i][1] - quadrantBounds[i][0] - mObstacles[i].width) / 100.0 +
            quadrantBounds[i][0];
        mObstacles[i].y =
            (rand() % 100) *
            (quadrantBounds[i][3] - quadrantBounds[i][2] - mObstacles[i].height) / 100.0 +
            quadrantBounds[i][2] + mObstacles[i].height;
    }

    // Randomize target quadrant (and place obstacle opposite)
    if (rand() % 2) {
        mTargetLocation.x =
            (rand() % 100) *
            (quadrantBounds[2][1] - 0.3 - max(quadrantBounds[2][0], mObstacles[1].x + mObstacles[1].width / 2 + 0.5) - TARGET_DIAMETER) / 100.0 +
            max(quadrantBounds[2][0], mObstacles[1].x + mObstacles[1].width / 2 + 0.5) + TARGET_DIAMETER;
        mTargetLocation.y =
            (rand() % 100) *
            (quadrantBounds[2][3] - quadrantBounds[2][2] - TARGET_DIAMETER) / 100.0 +
            quadrantBounds[2][2] + TARGET_DIAMETER;
        mObstacles[2].x =
            (rand() % 100) *
            (quadrantBounds[3][1] - quadrantBounds[3][0] - mObstacles[2].width) / 100.0 +
            quadrantBounds[3][0];
        mObstacles[2].y = max(
            (rand() % 100) *
            (quadrantBounds[3][3] - quadrantBounds[3][2] - mObstacles[2].height) / 100.0 +
            quadrantBounds[3][2] + mObstacles[2].height,
            mTargetLocation.y + TARGET_DIAMETER / 2 + 0.5
        );
    } else {
        mTargetLocation.x =
            (rand() % 100) *
            (quadrantBounds[3][1] - 0.3 - max(quadrantBounds[3][0], mObstacles[0].x + mObstacles[0].width / 2 + 0.5) - TARGET_DIAMETER) / 100.0 +
            max(quadrantBounds[3][0], mObstacles[0].x + mObstacles[0].width / 2 + 0.5) + TARGET_DIAMETER;
        mTargetLocation.y =
            (rand() % 100) *
            (quadrantBounds[3][3] - quadrantBounds[3][2] - TARGET_DIAMETER) / 100.0 +
            quadrantBounds[3][2] + TARGET_DIAMETER;
        mObstacles[2].x =
            (rand() % 100) *
            (quadrantBounds[2][1] - quadrantBounds[2][0] - mObstacles[2].width) / 100.0 +
            quadrantBounds[2][0];
        mObstacles[2].y = min(
            (rand() % 100) *
            (quadrantBounds[2][3] - quadrantBounds[2][2] - mObstacles[2].height) / 100.0 +
            quadrantBounds[2][2] + mObstacles[2].height,
            mTargetLocation.y - TARGET_DIAMETER / 2 - 0.5
        );
    }
    mObstaclesMutex.unlock();
    mDestinationMutex.unlock();
}

void Arena::setSize(float width, float height) {
    mArenaMutex.lock();
    mWidthM = width;
    mHeightM = height;
    mArenaMutex.unlock();
}

// Translates an Aruco Marker into a VS Marker and highlights in the image
Marker Arena::translate(aruco::Marker m) {
    mArenaMutex.lock();
    // Calculate theta of the marker by comparing the degree of the line created
    // by two corners with the degree of the arena
    float theta = mTheta - atan2(m[1].y - m[0].y, m[1].x - m[0].x);

    // Subtract away the origin
    float fx = m[0].x - mOriginPx[0];
    float fy = mOriginPx[1] - m[0].y;

    // Convert camera frame of reference to arena frame of reference
    float A = fx * cos(mTheta) + fy * sin(mTheta);
    float B = fy * cos(mTheta) - fx * sin(mTheta);

    // Shift measurement to center of marker
    float markerSide = sqrt((m[1].x - m[0].x)*(m[1].x - m[0].x) + (m[1].y - m[0].y)*(m[1].y - m[0].y));
    if (cos(theta) >= 0) {
        A += sqrt(2) * markerSide / 2 * cos(PI/4 - theta);
        B -= sqrt(2) * markerSide / 2 * sin(PI/4 - theta);
    } else {
        A -= sqrt(2) * markerSide / 2 * sin(theta - 3*PI/4);
        B += sqrt(2) * markerSide / 2 * cos(theta - 3*PI/4);
    }

    // Convert to meters and store into Marker
    float x = A / mPpm;
    float y = B / mPpm;
    mArenaMutex.unlock();
    return Marker(m.id, x, y, theta);
}

Position Arena::getTargetLocation(){
    QMutexLocker lock(&mDestinationMutex);
    return mTargetLocation;
}

void Arena::onCustomXChanged(double x) {
    mCustomMutex.lock();
    mCustomCoordinate.x = static_cast<float>(x);
    mCustomMutex.unlock();
}

void Arena::onCustomYChanged(double y) {
    mCustomMutex.lock();
    mCustomCoordinate.y = static_cast<float>(y);
    mCustomMutex.unlock();
}

void Arena::onDrawCustomChanged(bool draw) {
    mCustomMutex.lock();
    mDrawCustom = draw;
    mCustomMutex.unlock();
}

void Arena::onDrawDestinationChanged(bool draw) {
    mDestinationMutex.lock();
    mDrawDestination = draw;
    mDestinationMutex.unlock();
}

void Arena::onDrawObstaclesChanged(bool draw) {
    mObstaclesMutex.lock();
    mDrawObstacles = draw;
    mObstaclesMutex.unlock();
}

inline float Arena::max(float a, float b) {
    return a > b ? a : b;
}

inline float Arena::min(float a, float b) {
    return a < b ? a : b;
}

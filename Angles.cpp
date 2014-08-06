#include "Angles.hpp"

bool Angles::isAngleBetween(irr::f32 angle, irr::f32 startAng, irr::f32 endAng) {

    //adjust angles to make start angle in range 0-360. Change both angles together, so their difference is maintained
    while (startAng < 0) {
        startAng+=360;
        endAng+=360;
    }
    while (startAng >= 360) {
        startAng-=360;
        endAng-=360;
    }

    //normalise angle
    angle = normaliseAngle(angle);

    //std::cout << angle << " " << startAng << " " << endAng << std::endl;

    if(endAng <= 360) { //Simple case
        return (angle >= startAng && angle <=endAng);
    } else { //End angle > 360
        return (angle >= startAng || angle <= normaliseAngle(endAng));
    }
}

irr::f32 Angles::normaliseAngle(irr::f32 angle) { //ensure angle is in range 0-360
    while (angle < 0) {
        angle+=360;
    }

    while (angle >= 360) {
        angle-=360;
    }

    return angle;
}
#include "RadarCalculation.hpp"

#include "Terrain.hpp"
#include "OwnShip.hpp"
#include "Buoys.hpp"
#include "OtherShips.hpp"
#include "RadarData.hpp"
#include "Angles.hpp"
#include "Constants.hpp"

#include <iostream>
#include <cmath>

using namespace irr;

RadarCalculation::RadarCalculation()
{
    //initialise scanArray size (360 x rangeResolution points per scan)
    rangeResolution = 64;
    scanArray.resize(360,std::vector<f32>(rangeResolution,0.0));

    currentScanAngle=0;

    cellLength = 20; //Fixme: This needs to change with radar range
    scanAngleStep=2;
}

RadarCalculation::~RadarCalculation()
{
    //dtor
}

void RadarCalculation::update(irr::video::IImage * radarImage, const Terrain& terrain, const OwnShip& ownShip, const Buoys& buoys, const OtherShips& otherShips, irr::f32 tideHeight)
{
    scan(terrain, ownShip, buoys, otherShips, tideHeight); // scan into scanArray[row (angle)][column (step)]
    render(radarImage,10); //From scanArray[row (angle)][column (step)], render to radarImage (Fixme: hardcoded amplification factor)
}

void RadarCalculation::scan(const Terrain& terrain, const OwnShip& ownShip, const Buoys& buoys, const OtherShips& otherShips, irr::f32 tideHeight)
{
    core::vector3df position = ownShip.getPosition();
    irr::f32 radarScannerHeight = 2.0;//Fixme: Hardcoding

    //Some tuning parameters
    irr::f32 radarFactorLand=2.0;
    irr::f32 radarFactorVessel=0.0001;

    //Load radar data for other contacts
    std::vector<RadarData> radarData;
    //For other ships
    for (std::vector<RadarData>::size_type contactID=1; contactID<=otherShips.getNumber(); contactID++) {
        radarData.push_back(otherShips.getRadarData(contactID,position));
    }
    //For buoys
    for (std::vector<RadarData>::size_type contactID=1; contactID<=buoys.getNumber(); contactID++) {
        radarData.push_back(buoys.getRadarData(contactID,position));
    }

    irr::u32 scansPerLoop = 3; //Fixme: Change this to get a constant configurable scan rate (within reason)
    for(int i = 0; i<scansPerLoop;i++) { //Start of repeatable scan section
        f32 scanSlope = 0.0; //Slope at start of scan
        for (int currentStep = 1; currentStep<rangeResolution; currentStep++) {
            //scan into array, accessed as  scanArray[row (angle)][column (step)]

            //Clear old value
            scanArray[currentScanAngle][currentStep] = 0.0;

            //Get location of area being scanned
            f32 localRange = cellLength*currentStep;
            f32 relX = localRange*sin(currentScanAngle*core::DEGTORAD); //Distance from ship
            f32 relZ = localRange*cos(currentScanAngle*core::DEGTORAD);
            f32 localX = position.X + relX;
            f32 localZ = position.Z + relZ;

            //get extents
            f32 minCellAngle = Angles::normaliseAngle(currentScanAngle - scanAngleStep/2.0);
            f32 maxCellAngle = Angles::normaliseAngle(currentScanAngle + scanAngleStep/2.0);
            f32 minCellRange = localRange - cellLength/2.0;
            f32 maxCellRange = localRange + cellLength/2.0;

            //get height, and adjustment for earth's curvature
            f32 dropWithCurvature = std::pow(localRange,2)/(2*EARTH_RAD_M*EARTH_RAD_CORRECTION);
            f32 radarHeight = terrain.getHeight(localX,localZ) - dropWithCurvature - radarScannerHeight - tideHeight;

            f32 localSlope = radarHeight/localRange;
            //Find height above previous maximum scan slope
            f32 heightAboveLine = radarHeight - scanSlope*localRange;

            //Scan other contacts here
            //Fixme: Implementation needs completing later for ARPA to check if contact is detectable against clutter
            for(std::vector<RadarData>::iterator it = radarData.begin(); it != radarData.end(); ++it) {
                //if( std::abs(it->relX-relX)<50.0 && std::abs(it->relZ-relZ)<50.0 ) {scanArray[currentScanAngle][currentStep] = 1.0;}
                f32 contactHeightAboveLine = (it->height - radarScannerHeight - dropWithCurvature) - scanSlope*localRange;
                if (contactHeightAboveLine > 0) {
                    //Contact would be visible if in this cell. Check if it is
                    //if( std::abs(it->relX-relX)<50.0 && std::abs(it->relZ-relZ)<50.0 ) {scanArray[currentScanAngle][currentStep] = 1.0;} //Initial test implementation
                    //Start of B3D code
                    if ((it->range >= minCellRange && it->range <= maxCellRange) || (it->minRange >= minCellRange && it->minRange <= maxCellRange) || (it->maxRange >= minCellRange && it->maxRange <= maxCellRange) || (it->minRange < minCellRange && it->maxRange > maxCellRange)) {//Check if centre of target within the cell. If not then check if Either min range or max range of contact is within the cell, or min and max span the cell
                        if ((Angles::isAngleBetween(it->angle,minCellAngle,maxCellAngle)) || (Angles::isAngleBetween(it->minAngle,minCellAngle,maxCellAngle)) || (Angles::isAngleBetween(it->maxAngle,minCellAngle,maxCellAngle)) || ( Angles::normaliseAngle(it->minAngle-minCellAngle) > 270 && Angles::normaliseAngle(it->maxAngle-maxCellAngle) < 90)) {//Check if centre of target within the cell. If not then check if either min angle or max angle of contact is within the cell, or min and max span the cell

                            irr::f32 rangeAtCellMin = rangeAtAngle(minCellAngle,it->relX,it->relZ,it->heading);
                            irr::f32 rangeAtCellMax = rangeAtAngle(maxCellAngle,it->relX,it->relZ,it->heading);

                            //check if the contact intersects this exact cell, if its extremes overlap it
                            //Also check if the target centre is in the cell, or the extended target spans the cell (ie RangeAtCellMin less than minCellRange and rangeAtCellMax greater than maxCellRange and vice versa)
                            if ((((it->range >= minCellRange && it->range <= maxCellRange) && (Angles::isAngleBetween(it->angle,minCellAngle,maxCellAngle))) || (rangeAtCellMin >= minCellRange && rangeAtCellMin <= maxCellRange) || (rangeAtCellMax >= minCellRange && rangeAtCellMax <= maxCellRange) || (rangeAtCellMin < minCellRange && rangeAtCellMax > maxCellRange) || (rangeAtCellMax < minCellRange && rangeAtCellMin > maxCellRange))){

                                irr::f32 radarEchoStrength = radarFactorVessel * std::pow(M_IN_NM/localRange,4) * it->rcs;
                                scanArray[currentScanAngle][currentStep] += radarEchoStrength;
                                /*
                                ;check how visible against noise/clutter. If visible, record as detected for ARPA tracking
                                If radarEchoStrength#*2 > radarNoiseValueNoBlock(radarNoiseLevel#, radarSeaClutter#, radarRainClutter#, weather#, AllRadarTargets(i)\range, rainIntensity)

                                    ;DebugLog "Contact:"
                                    ;DebugLog Str(radarNoiseValueNoBlock(radarNoiseLevel#, radarSeaClutter#, radarRainClutter#, weather#, AllRadarTargets(i)\range, rainIntensity))
                                    ;DebugLog radarEchoStrength#*2

                                    contactLastDetected(i) = absolute_time
                                EndIf

                                RadarIntensity#(Int(radarBrg#),RadarCurrentStep) = RadarIntensity#(Int(radarBrg#),RadarCurrentStep) + radarEchoStrength# ;add target reflection to array

                                ;RACON code
                                ;make an echo line behind the contact
                                If AllRadarTargets(i)\racon <> ""

                                    If Float(time#+AllRadarTargets(i)\raconOffsetTime) Mod 60 <= RaconOnTime# ;Show for RaconOnTime# seconds per minute

                                        Local raconEchoStrength# = radarFactorRACON * (1852/radarRange#)^2;RACON/SART goes with inverse square law as we are receiving the direct signal, not echo

                                        ;set start point for racon echo (global variable)
                                        raconCurrentStep = RadarCurrentStep

                                        addRaconString(raconEchoStrength, radarBrg#, 750, radarStep#, AllRadarTargets(i)\racon$)

                                    EndIf

                                EndIf
                                */
                                //if a target entirely covers the angle of a cell, then use its blocking height and increase radarHeight, so it blocks reflections from behind
                                if ( Angles::normaliseAngle(it->minAngle-minCellAngle) > 270 && Angles::normaliseAngle(it->maxAngle-maxCellAngle) < 90) {
                                    //reset maxRadarHeight to new value if the solid height is higher
                                    scanSlope = std::max(scanSlope,(it->solidHeight-radarScannerHeight-dropWithCurvature)/localRange);

                                }
                            }


                        }
                    }
                    //End of B3D code
                }
            }


            //ToDo Add radar noise

            //Add land scan
            if (heightAboveLine>0) {
                f32 radarLocalGradient = heightAboveLine/cellLength;
                scanSlope = localSlope; //Highest so far on scan
                scanArray[currentScanAngle][currentStep] += radarFactorLand*std::atan(radarLocalGradient)*(2/PI)/std::pow(localRange/M_IN_NM,3); //make a reflection off a plane wall at 1nm have a magnitude of 1*radarFactorLand
            }

        } //End of for loop scanning out

        //Increment scan angle for next time
        currentScanAngle += scanAngleStep;
        if (currentScanAngle>=360) {
            currentScanAngle=0;
        }
    } //End of repeatable scan section
}

void RadarCalculation::render(irr::video::IImage * radarImage, irr::f32 amplification)
{
    //*************************
    //generate image from array
    //*************************

    //Fill with background colour
    radarImage->fill(video::SColor(255, 0, 0, 255));

    //Get image size
    int bitmapWidth = radarImage->getDimension().Width;
    if (radarImage->getDimension().Height != bitmapWidth)
        {return;} //Check image is square, and return without action if not

    //draw from array to image
    f32 centrePixel = (bitmapWidth-1.0)/2.0; //The centre of the bitmap. Normally this will be a fractional number (##.5)
    for (int scanAngle = 0; scanAngle <360; scanAngle+=scanAngleStep) {
        for (int currentStep = 1; currentStep<rangeResolution; currentStep++) {

            irr::f32 cellMinAngle = scanAngle - scanAngleStep/2.0;
            irr::f32 cellMaxAngle = scanAngle + scanAngleStep/2.0;
            irr::f32 cellMinRange = ((currentStep-0.5)*(bitmapWidth*0.5/(float)rangeResolution));//Range in pixels from centre
            irr::f32 cellMaxRange = ((currentStep+0.5)*(bitmapWidth*0.5/(float)rangeResolution));//Fixme: Check rounding etc

            u32 pixelColour=255*amplification*scanArray[scanAngle][currentStep];
            if (pixelColour>255) {pixelColour = 255;}

            drawSector(radarImage,centrePixel,centrePixel,cellMinRange,cellMaxRange,cellMinAngle,cellMaxAngle,255,pixelColour,pixelColour,0);

        }
    }
}

void RadarCalculation::drawSector(irr::video::IImage * radarImage,irr::f32 centreX, irr::f32 centreY, irr::f32 innerRadius, irr::f32 outerRadius, irr::f32 startAngle, irr::f32 endAngle, irr::u32 alpha, irr::u32 red, irr::u32 green, irr::u32 blue)
//draw a bounded sector
{
    //find the corner points (Fixme: Not quite right when the extreme point is on the outer curve)
    irr::f32 point1X = centreX + std::sin(irr::core::DEGTORAD*startAngle)*innerRadius;
    irr::f32 point1Y = centreY - std::cos(irr::core::DEGTORAD*startAngle)*innerRadius;
    irr::f32 point2X = centreX + std::sin(irr::core::DEGTORAD*startAngle)*outerRadius;
    irr::f32 point2Y = centreY - std::cos(irr::core::DEGTORAD*startAngle)*outerRadius;
    irr::f32 point3X = centreX + std::sin(irr::core::DEGTORAD*endAngle)*outerRadius;
    irr::f32 point3Y = centreY - std::cos(irr::core::DEGTORAD*endAngle)*outerRadius;

    irr::f32 point4X = centreX + std::sin(irr::core::DEGTORAD*endAngle)*innerRadius;
    irr::f32 point4Y = centreY - std::cos(irr::core::DEGTORAD*endAngle)*innerRadius;

    //find the 'bounding box'
    irr::f32 minX = std::min(std::min(point1X,point2X),std::min(point3X,point4X));
    irr::f32 maxX = std::max(std::max(point1X,point2X),std::max(point3X,point4X));
    irr::f32 minY = std::min(std::min(point1Y,point2Y),std::min(point3Y,point4Y));
    irr::f32 maxY = std::max(std::max(point1Y,point2Y),std::max(point3Y,point4Y));

    irr::f32 innerRadiusSqr = std::pow(innerRadius,2);
    irr::f32 outerRadiusSqr = std::pow(outerRadius,2);

    //draw the points
    for (int i = minX;i<=maxX;i++) {
        for (int j = minY;j<=maxY;j++) {

            irr::f32 localX = i - centreX; //position referred to centre
            irr::f32 localY = j - centreY; //position referred to centre

            irr::f32 localRadiusSqr = std::pow(localX,2) + std::pow(localY,2); //check radius of points
            irr::f32 localAngle = irr::core::RADTODEG*std::atan2(localX,-1*localY); //check angle of point

            //if the point is within the limits, plot it
            if (localRadiusSqr >= innerRadiusSqr && localRadiusSqr <= outerRadiusSqr) {
                if (Angles::isAngleBetween(localAngle,startAngle,endAngle)) {
                    //Plot i,j
                    radarImage->setPixel(i,j,video::SColor(alpha,red,green,blue));
                }
            }
        }
    }
}

irr::f32 RadarCalculation::rangeAtAngle(irr::f32 checkAngle,irr::f32 centreX, irr::f32 centreZ, irr::f32 heading)
{
	//Special case is if heading and checkAngle are identical. In this case, return the centre point if it lies on the angle, and 0 if not
	if (std::abs(Angles::normaliseAngle(checkAngle-heading)) < 0.001) {
		if (Angles::normaliseAngle(irr::core::RADTODEG*std::atan2(centreX,centreZ)-checkAngle) < 0.1) {
			return std::sqrt(std::pow(centreX,2) + std::pow(centreZ,2));
		} else {
			return 0;
		}
	}

	irr::f32 lambda; //This is the distance from the centre of the contact

	lambda = (centreX - centreZ*tan(irr::core::DEGTORAD*checkAngle))/(cos(irr::core::DEGTORAD*heading)*tan(irr::core::DEGTORAD*checkAngle) - sin(irr::core::DEGTORAD*heading));

	irr::f32 distanceSqr = std::pow(lambda,2) + lambda*(2*centreX*sin(irr::core::DEGTORAD*heading) + 2*centreZ*cos(irr::core::DEGTORAD*heading)) + (std::pow(centreX,2) + std::pow(centreZ,2));

	irr::f32 distance = 0;

	if (distanceSqr > 0) {
		distance = std::sqrt(distanceSqr);
	}

	return distance;

}
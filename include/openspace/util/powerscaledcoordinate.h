/****************************************************************************************
*                                                                                       *
*                                                                                       *
*                                  This is MINE,                                        *
*                                                                                       *
*                                  ALL MINE!!!!                                         *
*                                                                                       *
*                                                                                       *
*                                                                                       *
*                                                                                       *
****************************************************************************************/


//Removin psc, rewriting. Better naming should be done. 



#ifndef __POWERSCALEDCOORDINATE_H__
#define __POWERSCALEDCOORDINATE_H__

// open space includes
// glm includes
#include <ghoul/glm.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>

namespace openspace
{

// forward declare the power scaled scalars
class PowerScaledScalar;

class PowerScaledCoordinate {
public:
// constructors
PowerScaledCoordinate();

PowerScaledCoordinate(PowerScaledCoordinate&& rhs);
PowerScaledCoordinate(const PowerScaledCoordinate& rhs);

// Sets the power scaled coordinates directly
PowerScaledCoordinate(glm::vec4 v);
PowerScaledCoordinate(float f1, float f2, float f3, float f4);
// Sets the power scaled coordinates with w = 0
PowerScaledCoordinate(glm::vec3 v);

static PowerScaledCoordinate CreatePowerScaledCoordinate(double d1, double d2, double d3);

// get functions
// return the full, unmodified PSC
const glm::vec4& vec4() const;

// returns the rescaled, "normal" coordinates
glm::vec3 vec3() const;

// return the full psc as dvec4()
glm::dvec4 dvec4() const;

// rescaled return as dvec3
glm::dvec3 dvec3() const;

// length of the vector as a pss
PowerScaledScalar length() const;
glm::vec3 direction() const;

// operator overloading
PowerScaledCoordinate& operator=(const PowerScaledCoordinate& rhs);
PowerScaledCoordinate& operator=(PowerScaledCoordinate&& rhs);
PowerScaledCoordinate& operator+=(const PowerScaledCoordinate& rhs);
PowerScaledCoordinate operator+(const PowerScaledCoordinate& rhs) const;
PowerScaledCoordinate& operator-=(const PowerScaledCoordinate& rhs);
PowerScaledCoordinate operator-(const PowerScaledCoordinate& rhs) const;
float& operator[](unsigned int idx);
float operator[](unsigned int idx) const;
double dot(const PowerScaledCoordinate& rhs) const;
double angle(const PowerScaledCoordinate& rhs) const;

// scalar operators
PowerScaledCoordinate operator*(const double& rhs) const;
PowerScaledCoordinate operator*(const float& rhs) const;
PowerScaledCoordinate& operator*=(const PowerScaledScalar& rhs);
PowerScaledCoordinate operator*(const PowerScaledScalar& rhs) const;
PowerScaledCoordinate operator*(const glm::mat4& matrix) const;


// comparison
bool operator==(const PowerScaledCoordinate& other) const;
bool operator!=(const PowerScaledCoordinate& other) const;
bool operator<(const PowerScaledCoordinate& other) const;
bool operator>(const PowerScaledCoordinate& other) const;
bool operator<=(const PowerScaledCoordinate& other) const;
bool operator>=(const PowerScaledCoordinate& other) const;

// glm integration
PowerScaledCoordinate& operator=(const glm::dvec4& rhs);
PowerScaledCoordinate& operator=(const glm::vec4& rhs);
PowerScaledCoordinate& operator=(const glm::dvec3& rhs);
PowerScaledCoordinate& operator=(const glm::vec3& rhs);

friend std::ostream& operator<<(std::ostream& os, const PowerScaledCoordinate& rhs);

// allow the power scaled scalars to access private members
friend class PowerScaledScalar;

private:
// internal glm vector
glm::vec4 _vec;
};

typedef PowerScaledCoordinate psc;

} // namespace openspace

#endif // __POWERSCALEDCOORDINATE_H__













//Old psc code:
/*
#ifndef __POWERSCALEDCOORDINATE_H__
#define __POWERSCALEDCOORDINATE_H__

// open space includes
// glm includes
#include <ghoul/glm.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>

namespace openspace
{

// forward declare the power scaled scalars
class PowerScaledScalar;

class PowerScaledCoordinate {
public:
    // constructors
    PowerScaledCoordinate();

    PowerScaledCoordinate(PowerScaledCoordinate&& rhs);
	PowerScaledCoordinate(const PowerScaledCoordinate& rhs);

    // Sets the power scaled coordinates directly
    PowerScaledCoordinate(glm::vec4 v);
    PowerScaledCoordinate(float f1, float f2, float f3, float f4);
    // Sets the power scaled coordinates with w = 0
    PowerScaledCoordinate(glm::vec3 v);

    static PowerScaledCoordinate CreatePowerScaledCoordinate(double d1, double d2, double d3);

    // get functions
    // return the full, unmodified PSC 
    const glm::vec4& vec4() const;

    // returns the rescaled, "normal" coordinates
    glm::vec3 vec3() const;

	// return the full psc as dvec4()
	 glm::dvec4 dvec4() const;

	// rescaled return as dvec3
	glm::dvec3 dvec3() const;

    // length of the vector as a pss
    PowerScaledScalar length() const;
    glm::vec3 direction() const;

    // operator overloading
    PowerScaledCoordinate& operator=(const PowerScaledCoordinate& rhs);
    PowerScaledCoordinate& operator=(PowerScaledCoordinate&& rhs);
    PowerScaledCoordinate& operator+=(const PowerScaledCoordinate& rhs);
    PowerScaledCoordinate operator+(const PowerScaledCoordinate& rhs) const;
    PowerScaledCoordinate& operator-=(const PowerScaledCoordinate& rhs);
    PowerScaledCoordinate operator-(const PowerScaledCoordinate& rhs) const;
    float& operator[](unsigned int idx);
    float operator[](unsigned int idx) const;
    double dot(const PowerScaledCoordinate& rhs) const;
    double angle(const PowerScaledCoordinate& rhs) const;

    // scalar operators
    PowerScaledCoordinate operator*(const double& rhs) const;
    PowerScaledCoordinate operator*(const float& rhs) const;
    PowerScaledCoordinate& operator*=(const PowerScaledScalar& rhs);
    PowerScaledCoordinate operator*(const PowerScaledScalar& rhs) const;
    PowerScaledCoordinate operator*(const glm::mat4& matrix) const;


    // comparison
    bool operator==(const PowerScaledCoordinate& other) const;
    bool operator!=(const PowerScaledCoordinate& other) const;
    bool operator<(const PowerScaledCoordinate& other) const;
    bool operator>(const PowerScaledCoordinate& other) const;
    bool operator<=(const PowerScaledCoordinate& other) const;
    bool operator>=(const PowerScaledCoordinate& other) const;

    // glm integration
    PowerScaledCoordinate& operator=(const glm::dvec4& rhs);
    PowerScaledCoordinate& operator=(const glm::vec4& rhs);
    PowerScaledCoordinate& operator=(const glm::dvec3& rhs);
    PowerScaledCoordinate& operator=(const glm::vec3& rhs);

    friend std::ostream& operator<<(std::ostream& os, const PowerScaledCoordinate& rhs);

    // allow the power scaled scalars to access private members
    friend class PowerScaledScalar;

private:
    // internal glm vector
    glm::vec4 _vec;
};

typedef PowerScaledCoordinate psc;

} // namespace openspace

#endif // __POWERSCALEDCOORDINATE_H__
*/
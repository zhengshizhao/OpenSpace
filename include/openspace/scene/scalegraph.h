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

#ifndef __SCALEGRAPH_H__
#define __SCALEGRAPH_H__

#include <openspace/util/powerscaledcoordinate.h>
#include <ghoul/misc/dictionary.h>
#include <openspace/util/updatestructures.h>

namespace openspace {

	class Scalegraph {
	public:
		static Scalegraph* createFromDictionary(const ghoul::Dictionary& dictionary);

		Scalegraph(const ghoul::Dictionary& dictionary);
		virtual ~Scalegraph();
		virtual bool initialize();
		virtual const glm::vec3& position() const = 0;
		virtual void update(const UpdateData& data);

	protected:
		Scalegraph();
	};

}  // namespace openspace

#endif // __SCALEGRAPH_H__

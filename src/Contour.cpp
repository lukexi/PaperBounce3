//
//  Contour.cpp
//  PaperBounce3
//
//  Created by Chaim Gingold on 8/4/16.
//
//

#include "Contour.h"
#include "geom.h"

const Contour* ContourVector::findClosestContour ( vec2 point, vec2* closestPoint, float* closestDist, ContourKind kind ) const
{
	float best = MAXFLOAT ;
	const Contour* result = 0 ;
	
	// can optimize this by using bounding boxes as heuristic, but whatev for now.
	for ( const auto &c : *this )
	{
		if ( c.isKind(kind) )
		{
			float dist ;
			
			vec2 x = closestPointOnPoly( point, c.mPolyLine, 0, 0, &dist ) ;
			
			if ( dist < best )
			{
				best = dist ;
				result = &c ;
				if (closestPoint) *closestPoint = x ;
				if (closestDist ) *closestDist  = dist ;
			}
		}
	}
	
	return result ;
}

const Contour* ContourVector::findLeafContourContainingPoint( vec2 point ) const
{
	function<const Contour*(const Contour&)> search = [&]( const Contour& at ) -> const Contour*
	{
		if ( at.contains(point) )
		{
			for( auto childIndex : at.mChild )
			{
				const Contour* x = search( (*this)[childIndex] ) ;
				
				if (x) return x ;
			}
			
			return &at ;
		}
		
		return 0 ;
	} ;

	for( const auto &c : *this )
	{
		if ( c.mTreeDepth == 0 )
		{
			const Contour* x = search(c) ;
			
			if (x) return x ;
		}
	}
	
	return 0 ;
}

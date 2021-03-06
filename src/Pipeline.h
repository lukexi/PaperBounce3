//
//  Pipeline.hpp
//  PaperBounce3
//
//  Created by Chaim Gingold on 8/10/16.
//
//

#ifndef Pipeline_hpp
#define Pipeline_hpp

#include "cinder/Surface.h"
#include "CinderOpenCV.h"

#include <string>
#include <vector>

using namespace ci;
using namespace std;

class Pipeline
{
	/*	Pipeline isn't the pipeline so much as helping us trace the pipeline's process.
		However, I'm sticking with this name as I hope that it can one day develop into
		both debugging insight into the pipeline, controls for it, and eventually a program
		that is the image munging pipeline itself.
	*/
	
public:
	
	bool empty() const { return mStages.empty(); }
	
	void	setQuery( string q ) { mQuery=q; } ;
	string	getQuery() const { return mQuery; }
	
	string  getFirstStageName() const;
	string  getLastStageName () const;
	
	string	getNextStageName( string s ) const { return getAdjStageName(s, 1); }
	string	getPrevStageName( string s ) const { return getAdjStageName(s,-1); }
	
	void setImageToWorldTransform( const glm::mat4& );
	
	// add types:
	// - contour
	// (maybe a class that slots into here and does drawing and eventually interaction)
	
	class Stage
	{
	public:
		string			mName;
		mat4			mImageToWorld;
		mat4			mWorldToImage;
		vec2			mImageSize;
		
		cv::Mat			mImageCV;
		mutable gl::TextureRef	mImageGL;
		
		gl::TextureRef getGLImage() const; // will convert mImageCV -> mImageGL if needed & possible.
		
		// layout hints
		float			mLayoutHintScale=1.f;
		bool			mLayoutHintOrtho=false; // keep laying out in the same row?
	};

	typedef shared_ptr<Stage> StageRef;


	void start();
	
	StageRef then( string name );
	StageRef then( string name, vec2 frameSize );

	StageRef then( string name, Surface &img );
	StageRef then( string name, cv::Mat &img );
	StageRef then( string name, gl::Texture2dRef ref );
	
	const vector<StageRef>& getStages() const { return mStages ; }
	const StageRef getStage( string name ) const;
	
	void setCaptureAllStageImages( bool v ) { mCaptureAllStageImages=v; }
	
	mat4 getCoordSpaceTransform( string from, string to ) const;
		// from/to is name of coordinate space.
		// these can refer to a Stage::mName, or "world"
	
private:

	bool mCaptureAllStageImages = false; // if false, then only extract query stage. true: capture all.
	
	void then( function<gl::Texture2dRef()>, string name, vec2 size );
		// so that we only fabricate the texture if it matches the query.
		// this might be a silly optimization, and we should just cache them all.

	bool	 getShouldCacheImage( const StageRef );
	
	string getAdjStageName( string, int adj ) const;

	vector<StageRef> mStages;
	
	string		   mQuery ;
	
} ;

#endif /* Pipeline_hpp */

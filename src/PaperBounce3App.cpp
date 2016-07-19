#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"
#include "cinder/Rand.h"

#include "cinder/Capture.h"
#include "CinderOpenCV.h"

using namespace ci;
using namespace ci::app;
using namespace std;


/*
 BUGS:
 - Need to remember what's a hole for what so that we can fix resolveCollision and pickContour; we won't get complex topologies right.
 */

//const cv::RetrievalModes kContourHierStyle = cv::RETR_TREE ;      // whole enchilada
const cv::RetrievalModes kContourHierStyle = cv::RETR_CCOMP ;     // is just two level, what we want
//const cv::RetrievalModes kContourHierStyle = cv::RETR_EXTERNAL ;

const float kResScale = 1.f ;
const float kContourMinRadius = 3.f  * kResScale ;
const float kContourMinArea   = 100.f * kResScale ;
const float kContourDPEpislon = 5.f  * kResScale ;
const float kContourMinWidth  = 5.f  * kResScale ;


const float kBallDefaultRadius = 8.f ;

//const vec2 kCaptureSize = vec2( 640, 480 ) ;
const vec2 kCaptureSize = vec2( 16.f/9.f * 480.f, 480 ) ;


const bool kAutoFullScreenProjector	= false ; // default: true
const bool kDrawCameraImage			= false ; // default: false
const bool kDrawContoursFilled		= true  ;  // default: false

namespace cinder {
	
	PolyLine2 fromOcv( vector<cv::Point> pts )
	{
		PolyLine2 pl;
		for( auto p : pts ) pl.push_back(vec2(p.x,p.y));
		pl.setClosed(true);
		return pl;
	}

}

class cWindowData {
public:
	bool mIsAux = false ;
};

class cBall {
	
public:
	vec2 mLoc ;
	vec2 mVel ;
	float mRadius ;
	ColorAf mColor ;
};

class cContour {
public:
	PolyLine2	mPolyLine ;
	vec2		mCenter ;
	Rectf		mBoundingRect ;
	float		mRadius ;
	float		mArea ;
	
	bool		mIsHole = false ;
};

class PaperBounce3App : public App {
  public:
	void setup() override;
	void mouseDown( MouseEvent event ) override;
	void mouseMove( MouseEvent event ) override { mMousePos = event.getPos(); }
	void update() override;
	void draw() override;
	void resize() override;
	void keyDown( KeyEvent event ) override;
	
	void drawProjectorWindow() ;
	void drawAuxWindow() ;

	void updateVision(); // updates mCameraTexture, mContours
	void updateBalls() ;
	

	CaptureRef				mCapture;
	gl::TextureRef			mCameraTexture;
	
	vector<cContour>		mContours;
	
	std::vector<cBall>		mBalls ;
	
	app::WindowRef			mAuxWindow ; // for other debug info, on computer screen
	
	double					mLastFrameTime = 0. ;
	vec2					mMousePos ;
	
	// physics/geometry helpers
	const cContour* pickContour ( vec2 point ) const ;
	const cContour* findClosestContour ( vec2 point, vec2* closestPoint ) const ; // assumes pickContour failed
	
	vec2 resolveCollisionWithContours	( vec2 p, float r ) const ; // returns pinned version of point
	vec2 resolveCollisionWithBalls		( vec2 p, float r, cBall* ignore=0 ) const ;
		// simple pushes p out of overlapping balls.
		// but might take multiple iterations to respond to all of them
	
	// for main window, the projector display
	vec2 mouseToWorld( vec2 );
	void updateWindowMapping();
	
	float	mOrthoRect[4]; // points for glOrtho
};

void PaperBounce3App::setup()
{
	mLastFrameTime = getElapsedSeconds() ;
	
	auto displays = Display::getDisplays() ;
	std::cout << displays.size() << " Displays" << std::endl ;
	for ( auto d : displays )
	{
		std::cout << "\t" << d->getBounds() << std::endl ;
	}
	
	mCapture = Capture::create( kCaptureSize.x, kCaptureSize.y );
	mCapture->start();

	// Fullscreen main window in second display
	if ( displays.size()>1 && kAutoFullScreenProjector )
	{
		getWindow()->setPos( displays[1]->getBounds().getUL() );
		
		getWindow()->setFullScreen(true) ;
	}

	// aux display
	if (0)
	{
		// for some reason this seems to create three windows once we fullscreen the main window :P
		app::WindowRef mAuxWindow = createWindow( Window::Format().size( kCaptureSize.x, kCaptureSize.y ) );
		
		cWindowData* data = new cWindowData ;
		data->mIsAux=true ;
		mAuxWindow->setUserData(data);
		
		if (displays.size()==1)
		{
			// move it out of the way on one screen
			mAuxWindow->setPos( getWindow()->getBounds().getUL() + ivec2(0,getWindow()->getBounds().getHeight() + 16.f) ) ;
		}
	}
	
}

void PaperBounce3App::mouseDown( MouseEvent event )
{
	cBall ball ;
	
	ball.mColor = ColorAf::hex(0xC62D41);
	ball.mLoc   = mouseToWorld( event.getPos() );
	ball.mRadius = kBallDefaultRadius ;
	
	ball.mRadius += Rand::randFloat(0.f,kBallDefaultRadius);
	
	mBalls.push_back( ball ) ;
}

void PaperBounce3App::update()
{
	updateVision();
	updateBalls();
}

void PaperBounce3App::updateVision()
{
	if( mCapture->checkNewFrame() )
	{
		// Get surface data
		Surface surface( *mCapture->getSurface() );
		mCameraTexture = gl::Texture::create( surface );
		
		// make cv frame
		cv::Mat input( toOcv( Channel( surface ) ) );
		cv::Mat output, gray, thresholded ;
		
//		cv::Sobel( input, output, CV_8U, 1, 0 );
		
		//		cv::threshold( input, output, 128, 255, CV_8U );
		//		cv::Laplacian( input, output, CV_8U );
		//		cv::circle( output, toOcv( Vec2f(200, 200) ), 300, toOcv( Color( 0, 0.5f, 1 ) ), -1 );
		//		cv::line( output, cv::Point( 1, 1 ), cv::Point( 30, 30 ), toOcv( Color( 1, 0.5f, 0 ) ) );

		
		// gray image
		if (1)
		{
//			cv::cvtColor( input, gray, cv::COLOR_BGR2GRAY ); // already grayscale
			
			cv::GaussianBlur( input, gray, cv::Size(5,5), 0 );

			cv::threshold( gray, thresholded, 0, 255, cv::THRESH_BINARY + cv::THRESH_OTSU );
		}
		
		// contours
		vector<vector<cv::Point> > contours;
		vector<cv::Vec4i> hierarchy;
		
		cv::findContours( thresholded, contours, hierarchy, kContourHierStyle, CV_CHAIN_APPROX_SIMPLE, cv::Point(0, 0) );
		
		// filter and process contours into our format
		mContours.clear() ;
		for( int i=0; i<contours.size(); ++i )
		{
			const auto &c = contours[i] ;
			
			cv::Point2f center ;
			float		radius ;
			
			cv::minEnclosingCircle( c, center, radius ) ;
			
			float		area = cv::contourArea(c) ;
			
			cv::RotatedRect rotatedRect = minAreaRect(c) ;
			
			if (	radius > kContourMinRadius &&
					area > kContourMinArea &&
					std::min( rotatedRect.size.width, rotatedRect.size.height ) > kContourMinWidth )
			{
				auto addContour = [&]( const vector<cv::Point>& c )
				{
					cContour myc ;
					
					myc.mPolyLine = fromOcv(c) ;
					myc.mRadius = radius ;
					myc.mCenter = fromOcv(center) ;
					myc.mArea   = area ;
					myc.mIsHole = hierarchy[i][3] != -1 ;
					myc.mBoundingRect = Rectf( myc.mPolyLine.getPoints() );
					
					mContours.push_back( myc );
				} ;
				
				if (1)
				{
					// simplify
					vector<cv::Point> approx ;
					
					cv::approxPolyDP( c, approx, kContourDPEpislon, true ) ;
					
					addContour(approx);
				}
				else addContour(c) ;
			}
		}
		
		// convert to texture
//		mCameraTexture = gl::Texture::create( fromOcv( output ), gl::Texture::Format().loadTopDown() );
//		mCameraTexture = gl::Texture::create( fromOcv( input ), gl::Texture::Format().loadTopDown() );
	}
}

void PaperBounce3App::updateBalls()
{
	for( auto &b : mBalls )
	{
		b.mLoc = resolveCollisionWithContours( b.mLoc, b.mRadius ) ;
		b.mLoc = resolveCollisionWithBalls   ( b.mLoc, b.mRadius, &b ) ;
	}
}

vec2 closestPointOnLineSeg ( vec2 p, vec2 a, vec2 b )
{
	vec2 ap = p - a ;
	vec2 ab = b - a ;
	
	float ab2 = ab.x*ab.x + ab.y*ab.y;
	float ap_ab = ap.x*ab.x + ap.y*ab.y;
	float t = ap_ab / ab2;
	
	if (t < 0.0f) t = 0.0f;
	else if (t > 1.0f) t = 1.0f;
	
	vec2 x = a + ab * t;
	return x ;
}

vec2 closestPointOnPoly( vec2 pt, const PolyLine2& poly, size_t *ai=0, size_t *bi=0, float* dist=0 )
{
	float best = MAXFLOAT ;
	vec2 result = pt ;
	
	// assume poly is closed
	for( size_t i=0; i<poly.size(); ++i )
	{
		size_t j = (i+1) % poly.size() ;
		
		vec2 a = poly.getPoints()[i];
		vec2 b = poly.getPoints()[j];

		vec2 x = closestPointOnLineSeg(pt, a, b);
		
		float dist = glm::distance(pt,x) ; // could eliminate sqrt

		if ( dist < best )
		{
			best = dist ;
			result = x ;
			if (ai) *ai = i ;
			if (bi) *bi = j ;
		}
	}
	
	if (dist) *dist = best ;
	return result ;
}

const cContour* PaperBounce3App::pickContour ( vec2 point ) const
{
	const cContour* pick=0 ;
	
	// inside a poly?
	for( const auto &c : mContours )
	{
		// optimization: skip non-holes if we are already in something
		if ( pick && !c.mIsHole ) continue ;
		
		// inside this poly?
		bool isInC = c.mBoundingRect.contains(point) && c.mPolyLine.contains(point) ;
		
		if ( isInC )
		{
			if ( c.mIsHole )
			{
				pick = 0 ;
				break ;
				// done, because of topological assumptions
				// BUG. actually this isn't true. if you have a thing in a hole. hmm.
			}
			else
			{
				pick = &c ;
			}
		}
	}
	
	return pick ;
}

const cContour* PaperBounce3App::findClosestContour ( vec2 point, vec2* closestPoint ) const
{
	float best = MAXFLOAT ;
	const cContour* result = 0 ;
	
	// can optimize this by using bounding boxes as heuristic, but whatev for now.
	for ( const auto &c : mContours )
	{
		float dist ;
		
		vec2 x = closestPointOnPoly( point, c.mPolyLine, 0, 0, &dist ) ;
		
		if ( dist < best )
		{
			best = dist ;
			result = &c ;
			if (closestPoint) *closestPoint = x ;
		}
	}
	
	return result ;
}

vec2 PaperBounce3App::resolveCollisionWithContours ( vec2 point, float radius ) const
{
	const cContour* inHole=0 ;
	const cContour* inPoly=0 ;
	// being inside a poly means we're OK (inside a piece of paper)
	// BUT we then should still test against holes to make sure...

	
	// inside a poly?
	for( const auto &c : mContours )
	{
		// optimization: skip non-holes if we are already in something
		// (BUG this might not be quite right in complex topologies)
		if ( inPoly && !c.mIsHole ) continue ;
		
		// inside this poly?
		bool isInC = c.mBoundingRect.contains(point) && c.mPolyLine.contains(point) ;
		
		if ( isInC )
		{
			if ( c.mIsHole )
			{
				inPoly = 0 ;
				inHole = &c;

				// BUG
				// this toplogical assumption is buggy in that it precludes paper inside of a hole.
				// we'll return to this issue later.
				break ;
			}
			else
			{
				inPoly = &c ;
				// don't break, because we still need to look for holes
				// future optimization (unnecessary) could be to remember which holes are in which shapes and only test those.
			}
		}
	}
	
	// ok, find closest
	if (inPoly)
	{
		// in paper, make sure we aren't overlapping the edge
		float dist ;
		vec2 x = closestPointOnPoly(point, inPoly->mPolyLine, 0, 0, &dist );

		vec2 p = point ;
		
		if ( dist < radius )
		{
			p = glm::normalize( point - x ) * radius + x ;
		}
			
		return p ;
	}
	else if ( inHole )
	{
		// push us out of this hole
		vec2 x = closestPointOnPoly(point, inHole->mPolyLine );

		return glm::normalize( x - point ) * radius + x ;
	}
	else
	{
		// push us into nearest paper
		vec2 x ;
		findClosestContour( point, &x ) ;
		
		return glm::normalize( x - point ) * radius + x ;
	}
}

vec2 PaperBounce3App::resolveCollisionWithBalls ( vec2 p, float r, cBall* ignore ) const
{
	for ( const auto &b : mBalls )
	{
		if ( &b==ignore ) continue ;
		
		float d = glm::distance(p,b.mLoc) ;
		
		float rs = r + b.mRadius ;
		
		if ( d < rs )
		{
			// just update p
			p = glm::normalize( p - b.mLoc ) * rs + b.mLoc ;
		}
	}
	
	return p ;
}

void PaperBounce3App::resize()
{
	updateWindowMapping();
}

void PaperBounce3App::updateWindowMapping()
{
	auto ortho = [&]( float a, float b, float c, float d )
	{
		mOrthoRect[0] = a ;
		mOrthoRect[1] = b ;
		mOrthoRect[2] = c ;
		mOrthoRect[3] = d ;
	};
	
	// set window transform
	{
		//
		const float captureAspectRatio = kCaptureSize.x / kCaptureSize.y ;
		const float windowAspectRatio  = (float)getWindowSize().x / (float)getWindowSize().y ;
		
		if ( captureAspectRatio < windowAspectRatio )
		{
			// vertical black bars
			float w = windowAspectRatio * kCaptureSize.y ;
			float barsw = w - kCaptureSize.x ;
			
			ortho( -barsw/2, kCaptureSize.x + barsw/2, kCaptureSize.y, 0.f ) ;
		}
		else if ( captureAspectRatio > windowAspectRatio )
		{
			// horizontal black bars
			float h = (1.f / windowAspectRatio) * kCaptureSize.x ;
			float barsh = h - kCaptureSize.y ;
			
			ortho( 0.f, kCaptureSize.x, kCaptureSize.y + barsh/2, -barsh/2 ) ;
		}
		else ortho( 0.f, kCaptureSize.x, kCaptureSize.y, 0.f ) ;
	}
}

vec2 PaperBounce3App::mouseToWorld( vec2 p )
{
	RectMapping rm( Rectf( 0.f, 0.f, getWindowSize().x, getWindowSize().y ),
					Rectf( mOrthoRect[0], mOrthoRect[3], mOrthoRect[1], mOrthoRect[2] ) ) ;
	
	return rm.map(p) ;
}

void PaperBounce3App::drawProjectorWindow()
{
	// set window transform
	gl::setMatrices( CameraOrtho( mOrthoRect[0], mOrthoRect[1], mOrthoRect[2], mOrthoRect[3], 0.f, 1.f ) ) ;
	
	// camera image
	if ( mCameraTexture && kDrawCameraImage )
	{
		gl::color( 1, 1, 1 );
		gl::draw( mCameraTexture );
	}
	
	// draw frame
	if (1)
	{
		gl::color( 1, 1, 1 );
		gl::drawStrokedRect( Rectf(0,0,kCaptureSize.x,kCaptureSize.y).inflated( vec2(-1,-1) ) ) ;
	}

	// draw contour bounding boxes, etc...
	{
		for( auto c : mContours )
		{
			if ( !c.mIsHole ) gl::color( 1.f, 1.f, 1.f, .2f ) ;
			else gl::color( 1.f, 0.f, .3f, .3f ) ;

			gl::drawStrokedRect(c.mBoundingRect);
		}
	}

	// draw contours
	{
		// filled
		if ( kDrawContoursFilled )
		{
			// solids
			for( auto c : mContours )
			{
				if ( !c.mIsHole ) gl::color(.2f,.2f,.4f,.5f);
				
				gl::drawSolid(c.mPolyLine);
			}

			// holes
			for( auto c : mContours )
			{
				if ( c.mIsHole ) gl::color(.0f,.0f,.0f,.8f);
				
				gl::drawSolid(c.mPolyLine);
			}
			
			// picked highlight
			vec2 mousePos = mouseToWorld(mMousePos) ;
			
			const cContour* picked = pickContour( mousePos ) ;
			
			if (picked)
			{
				gl::color(.2f,.6f,.4f,.8f);
				gl::draw(picked->mPolyLine);
				
				vec2 x = closestPointOnPoly(mousePos,picked->mPolyLine);
				gl::color(.5f,.1f,.3f,.9f);
				gl::drawSolidCircle(x, 5.f);
			}
		}
		// outlines
		else
		{
			for( auto c : mContours )
			{
				ColorAf color ;
				
				if ( !c.mIsHole ) color = ColorAf(1,1,1); //color = ColorAf::hex( 0x43730F ) ;
				else color = ColorAf::hex( 0xF19878 ) ;
				
	//			color = ColorAf(1,1,1);
				
				gl::color(color) ;
				gl::draw(c.mPolyLine) ;
			}
		}
	}
	
	// draw balls
	{
		for( auto b : mBalls )
		{
			gl::color(b.mColor) ;
			gl::drawSolidCircle( b.mLoc, b.mRadius ) ;
		}
	}
	
	// test collision logic
	{
		vec2 pt = mouseToWorld( mMousePos ) ;
		
		float r = kBallDefaultRadius ;
		
		vec2 fixed = resolveCollisionWithContours(pt,r);
		
		gl::color( ColorAf(0.f,0.f,1.f) ) ;
		gl::drawStrokedCircle(fixed,r);
		
		gl::color( ColorAf(0.f,1.f,0.f) ) ;
		gl::drawLine(pt, fixed);
	}
}

void PaperBounce3App::drawAuxWindow()
{
	gl::setMatricesWindow( kCaptureSize.x, kCaptureSize.y );
	gl::color( 1, 1, 1 );
	gl::draw( mCameraTexture );
}

void PaperBounce3App::draw()
{
	gl::clear();
	gl::enableAlphaBlending();
	gl::enable( GL_LINE_SMOOTH );
	gl::enable( GL_POLYGON_SMOOTH );
	//	gl::enable( gl_point_smooth );
	glHint( GL_LINE_SMOOTH_HINT, GL_NICEST );
	glHint( GL_POLYGON_SMOOTH_HINT, GL_NICEST );
	//	glHint( GL_POINT_SMOOTH_HINT, GL_NICEST );
	

	cWindowData* winData = getWindow()->getUserData<cWindowData>() ;
	
	if ( winData && winData->mIsAux )
	{
		drawAuxWindow();
	}
	else
	{
		drawProjectorWindow();
	}
}

void PaperBounce3App::keyDown( KeyEvent event )
{
	switch ( event.getCode() )
	{
		case KeyEvent::KEY_f:
			std::cout << "Frame rate: " << getFrameRate() << std::endl ;
			break ;
	}
}


CINDER_APP( PaperBounce3App, RendererGl, [&]( App::Settings *settings ) {
	settings->setFrameRate(120.f);
	settings->setWindowSize(kCaptureSize);
	settings->setTitle("See Paper") ;
})
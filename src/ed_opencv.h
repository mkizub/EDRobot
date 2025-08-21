//
// Created by mkizub on 16.08.2025.
//

#pragma once

#ifndef EDROBOT_ED_OPENCV_H
#define EDROBOT_ED_OPENCV_H

#include "opencv2/opencv.hpp"

namespace cv {

template<typename _Tp> class Line_
{
public:
    typedef _Tp value_type;

    //! default constructor
    Line_();
    Line_(_Tp _x0, _Tp _y0, _Tp _x1, _Tp _y1);
    Line_(const Line_& pt) = default;
    Line_(Line_&& pt) CV_NOEXCEPT = default;
    Line_(const Vec<_Tp, 4>& v);
    Line_(const Point_<_Tp>& pt0, const Point_<_Tp>& pt1);

    Line_& operator = (const Line_& ln) = default;
    Line_& operator = (Line_&& ln) CV_NOEXCEPT = default;
    //! conversion to another data type
    template<typename _Tp2> operator Line_<_Tp2>() const;

    //! conversion to the old-style C structures
    operator Vec<_Tp, 4>() const;

    //! get points
    Point_<_Tp> p0() const;
    Point_<_Tp> p1() const;

    //! true if empty
    bool empty() const;

    _Tp x0; //!< x coordinate of the point
    _Tp y0; //!< y coordinate of the point
    _Tp x1; //!< x coordinate of the point
    _Tp y1; //!< y coordinate of the point
};

typedef Line_<int> Line2i;
typedef Line_<int64> Line2l;
typedef Line_<float> Line2f;
typedef Line_<double> Line2d;
typedef Line2i Line;

template<typename _Tp> class DataType< Line_<_Tp> >
{
public:
    typedef Line_<_Tp>                               value_type;
    typedef Line_<typename DataType<_Tp>::work_type> work_type;
    typedef _Tp                                      channel_type;

    enum { generic_type = 0,
        channels     = 4,
        fmt          = traits::SafeFmt<channel_type>::fmt + ((channels - 1) << 8)
    };

    typedef Vec<channel_type, channels> vec_type;
};

namespace traits {
template<typename _Tp>
struct Depth< Line_<_Tp> > { enum { value = Line_<_Tp>::value }; };
template<typename _Tp>
struct Type< Line_<_Tp> > { enum { value = CV_MAKETYPE(Line_<_Tp>::value, 4) }; };
} // namespace

////////////////////////////////// Line /////////////////////////////////

template<typename _Tp> inline
Line_<_Tp>::Line_()
        : x0(0), y0(0), x1(0), y1(0) {}

template<typename _Tp> inline
Line_<_Tp>::Line_(_Tp _x0, _Tp _y0, _Tp _x1, _Tp _y1)
        : x0(_x0), y0(_y0), x1(_x1), y1(_y1) {}

template<typename _Tp> inline
Line_<_Tp>::Line_(const Point_<_Tp>& pt0, const Point_<_Tp>& pt1)
        : x0(pt0.x), y0(pt0.y), x1(pt1.x), y1(pt1.y) {}

template<typename _Tp> inline
Line_<_Tp>::Line_(const Vec<_Tp, 4>& v)
        : x0(v[0]), y0(v[1]), x1(v[2]), y1(v[3]) {}

template<typename _Tp> template<typename _Tp2> inline
Line_<_Tp>::operator Line_<_Tp2>() const
{
    return Line_<_Tp2>(saturate_cast<_Tp2>(x0), saturate_cast<_Tp2>(y0), saturate_cast<_Tp2>(x1), saturate_cast<_Tp2>(y1));
}


template<typename _Tp> inline
Point_<_Tp> Line_<_Tp>::p0() const
{
    return Point_<_Tp>(x0,y0);
}

template<typename _Tp> inline
Point_<_Tp> Line_<_Tp>::p1() const
{
    return Point_<_Tp>(x1,y1);
}

template<typename _Tp> inline
bool Line_<_Tp>::empty() const
{
    return x0 == x1 && y0 == y1;
}



}

#endif //EDROBOT_ED_OPENCV_H

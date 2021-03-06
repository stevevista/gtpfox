// Copyright (C) 2012  Davis E. King (davis@dlib.net)
// License: Boost Software License   See LICENSE.txt for the full license.
#ifndef DLIB_INTERPOlATIONh_
#define DLIB_INTERPOlATIONh_ 

#include "interpolation_abstract.h"
#include "../pixel.h"
#include "../matrix.h"
#include "assign_image.h"
#include <limits>
#include "../simd.h"
#include "../rand.h"

namespace dlib
{

// ----------------------------------------------------------------------------------------

    template <typename T>
    struct sub_image_proxy
    {
        sub_image_proxy() = default;

        sub_image_proxy (
            T& img,
            rectangle rect
        ) 
        {
            rect = rect.intersect(get_rect(img));
            typedef typename image_traits<T>::pixel_type pixel_type;

            _nr = rect.height();
            _nc = rect.width();
            _width_step = width_step(img);
            _data = (char*)image_data(img) + sizeof(pixel_type)*rect.left() + rect.top()*_width_step;
        }

        void* _data = 0;
        long _width_step = 0;
        long _nr = 0;
        long _nc = 0;
    };

    template <typename T>
    struct const_sub_image_proxy
    {
        const_sub_image_proxy() = default;

        const_sub_image_proxy (
            const T& img,
            rectangle rect
        ) 
        {
            rect = rect.intersect(get_rect(img));
            typedef typename image_traits<T>::pixel_type pixel_type;

            _nr = rect.height();
            _nc = rect.width();
            _width_step = width_step(img);
            _data = (const char*)image_data(img) + sizeof(pixel_type)*rect.left() + rect.top()*_width_step;
        }

        const void* _data = 0;
        long _width_step = 0;
        long _nr = 0;
        long _nc = 0;
    };

    template <typename T>
    struct image_traits<sub_image_proxy<T> >
    {
        typedef typename image_traits<T>::pixel_type pixel_type;
    };
    template <typename T>
    struct image_traits<const sub_image_proxy<T> >
    {
        typedef typename image_traits<T>::pixel_type pixel_type;
    };
    template <typename T>
    struct image_traits<const_sub_image_proxy<T> >
    {
        typedef typename image_traits<T>::pixel_type pixel_type;
    };
    template <typename T>
    struct image_traits<const const_sub_image_proxy<T> >
    {
        typedef typename image_traits<T>::pixel_type pixel_type;
    };

    template <typename T>
    inline long num_rows( const sub_image_proxy<T>& img) { return img._nr; }
    template <typename T>
    inline long num_columns( const sub_image_proxy<T>& img) { return img._nc; }

    template <typename T>
    inline long num_rows( const const_sub_image_proxy<T>& img) { return img._nr; }
    template <typename T>
    inline long num_columns( const const_sub_image_proxy<T>& img) { return img._nc; }

    template <typename T>
    inline void* image_data( sub_image_proxy<T>& img) 
    { 
        return img._data; 
    } 
    template <typename T>
    inline const void* image_data( const sub_image_proxy<T>& img) 
    {
        return img._data; 
    }

    template <typename T>
    inline const void* image_data( const const_sub_image_proxy<T>& img) 
    {
        return img._data; 
    }

    template <typename T>
    inline long width_step(
        const sub_image_proxy<T>& img
    ) { return img._width_step; }

    template <typename T>
    inline long width_step(
        const const_sub_image_proxy<T>& img
    ) { return img._width_step; }

    template <typename T>
    void set_image_size(sub_image_proxy<T>& img, long rows, long cols)
    {
        DLIB_CASSERT(img._nr == rows && img._nc == cols, "A sub_image can't be resized."
            << "\n\t img._nr: "<< img._nr
            << "\n\t img._nc: "<< img._nc
            << "\n\t rows:    "<< rows
            << "\n\t cols:    "<< cols
            );
    }

    template <
        typename image_type
        >
    sub_image_proxy<image_type> sub_image (
        image_type& img,
        const rectangle& rect
    )
    {
        return sub_image_proxy<image_type>(img,rect);
    }

    template <
        typename image_type
        >
    const const_sub_image_proxy<image_type> sub_image (
        const image_type& img,
        const rectangle& rect
    )
    {
        return const_sub_image_proxy<image_type>(img,rect);
    }

    template <typename T>
    inline sub_image_proxy<matrix<T>> sub_image (
        T* img,
        long nr,
        long nc,
        long row_stride
    )
    {
        sub_image_proxy<matrix<T>> tmp;
        tmp._data = img;
        tmp._nr = nr;
        tmp._nc = nc;
        tmp._width_step = row_stride*sizeof(T);
        return tmp;
    }

    template <typename T>
    inline const const_sub_image_proxy<matrix<T>> sub_image (
        const T* img,
        long nr,
        long nc,
        long row_stride
    )
    {
        const_sub_image_proxy<matrix<T>> tmp;
        tmp._data = img;
        tmp._nr = nr;
        tmp._nc = nc;
        tmp._width_step = row_stride*sizeof(T);
        return tmp;
    }

// ----------------------------------------------------------------------------------------
// ----------------------------------------------------------------------------------------

    class interpolate_nearest_neighbor
    {
    public:

        template <typename image_view_type, typename pixel_type>
        bool operator() (
            const image_view_type& img,
            const dlib::point& p,
            pixel_type& result
        ) const
        {
            COMPILE_TIME_ASSERT(pixel_traits<typename image_view_type::pixel_type>::has_alpha == false);

            if (get_rect(img).contains(p))
            {
                assign_pixel(result, img[p.y()][p.x()]);
                return true;
            }
            else
            {
                return false;
            }
        }

    };

// ----------------------------------------------------------------------------------------

    class interpolate_bilinear
    {
        template <typename T>
        struct is_rgb_image 
        {
            const static bool value = pixel_traits<typename T::pixel_type>::rgb;
        };

    public:

        template <typename T, typename image_view_type, typename pixel_type>
        typename disable_if<is_rgb_image<image_view_type>,bool>::type operator() (
            const image_view_type& img,
            const dlib::vector<T,2>& p,
            pixel_type& result
        ) const
        {
            COMPILE_TIME_ASSERT(pixel_traits<typename image_view_type::pixel_type>::has_alpha == false);

            const long left   = static_cast<long>(std::floor(p.x()));
            const long top    = static_cast<long>(std::floor(p.y()));
            const long right  = left+1;
            const long bottom = top+1;


            // if the interpolation goes outside img 
            if (!(left >= 0 && top >= 0 && right < img.nc() && bottom < img.nr()))
                return false;

            const double lr_frac = p.x() - left;
            const double tb_frac = p.y() - top;

            double tl = 0, tr = 0, bl = 0, br = 0;

            assign_pixel(tl, img[top][left]);
            assign_pixel(tr, img[top][right]);
            assign_pixel(bl, img[bottom][left]);
            assign_pixel(br, img[bottom][right]);
            
            double temp = (1-tb_frac)*((1-lr_frac)*tl + lr_frac*tr) + 
                              tb_frac*((1-lr_frac)*bl + lr_frac*br);
                            
            assign_pixel(result, temp);
            return true;
        }

        template <typename T, typename image_view_type, typename pixel_type>
        typename enable_if<is_rgb_image<image_view_type>,bool>::type operator() (
            const image_view_type& img,
            const dlib::vector<T,2>& p,
            pixel_type& result
        ) const
        {
            COMPILE_TIME_ASSERT(pixel_traits<typename image_view_type::pixel_type>::has_alpha == false);

            const long left   = static_cast<long>(std::floor(p.x()));
            const long top    = static_cast<long>(std::floor(p.y()));
            const long right  = left+1;
            const long bottom = top+1;


            // if the interpolation goes outside img 
            if (!(left >= 0 && top >= 0 && right < img.nc() && bottom < img.nr()))
                return false;

            const double lr_frac = p.x() - left;
            const double tb_frac = p.y() - top;

            double tl, tr, bl, br;

            tl = img[top][left].red;
            tr = img[top][right].red;
            bl = img[bottom][left].red;
            br = img[bottom][right].red;
            const double red = (1-tb_frac)*((1-lr_frac)*tl + lr_frac*tr) + 
                                   tb_frac*((1-lr_frac)*bl + lr_frac*br);

            tl = img[top][left].green;
            tr = img[top][right].green;
            bl = img[bottom][left].green;
            br = img[bottom][right].green;
            const double green = (1-tb_frac)*((1-lr_frac)*tl + lr_frac*tr) + 
                                   tb_frac*((1-lr_frac)*bl + lr_frac*br);

            tl = img[top][left].blue;
            tr = img[top][right].blue;
            bl = img[bottom][left].blue;
            br = img[bottom][right].blue;
            const double blue = (1-tb_frac)*((1-lr_frac)*tl + lr_frac*tr) + 
                                   tb_frac*((1-lr_frac)*bl + lr_frac*br);
                            
            rgb_pixel temp;
            assign_pixel(temp.red, red);
            assign_pixel(temp.green, green);
            assign_pixel(temp.blue, blue);
            assign_pixel(result, temp);
            return true;
        }
    };

// ----------------------------------------------------------------------------------------

    class interpolate_quadratic
    {
        template <typename T>
        struct is_rgb_image 
        {
            const static bool value = pixel_traits<typename T::pixel_type>::rgb;
        };

    public:

        template <typename T, typename image_view_type, typename pixel_type>
        typename disable_if<is_rgb_image<image_view_type>,bool>::type operator() (
            const image_view_type& img,
            const dlib::vector<T,2>& p,
            pixel_type& result
        ) const
        {
            COMPILE_TIME_ASSERT(pixel_traits<typename image_view_type::pixel_type>::has_alpha == false);

            const point pp(p);

            // if the interpolation goes outside img 
            if (!get_rect(img).contains(grow_rect(pp,1))) 
                return false;

            const long r = pp.y();
            const long c = pp.x();

            const double temp = interpolate(p-pp, 
                                    img[r-1][c-1],
                                    img[r-1][c  ],
                                    img[r-1][c+1],
                                    img[r  ][c-1],
                                    img[r  ][c  ],
                                    img[r  ][c+1],
                                    img[r+1][c-1],
                                    img[r+1][c  ],
                                    img[r+1][c+1]);

            assign_pixel(result, temp);
            return true;
        }

        template <typename T, typename image_view_type, typename pixel_type>
        typename enable_if<is_rgb_image<image_view_type>,bool>::type operator() (
            const image_view_type& img,
            const dlib::vector<T,2>& p,
            pixel_type& result
        ) const
        {
            COMPILE_TIME_ASSERT(pixel_traits<typename image_view_type::pixel_type>::has_alpha == false);

            const point pp(p);

            // if the interpolation goes outside img 
            if (!get_rect(img).contains(grow_rect(pp,1))) 
                return false;

            const long r = pp.y();
            const long c = pp.x();

            const double red = interpolate(p-pp, 
                            img[r-1][c-1].red,
                            img[r-1][c  ].red,
                            img[r-1][c+1].red,
                            img[r  ][c-1].red,
                            img[r  ][c  ].red,
                            img[r  ][c+1].red,
                            img[r+1][c-1].red,
                            img[r+1][c  ].red,
                            img[r+1][c+1].red);
            const double green = interpolate(p-pp, 
                            img[r-1][c-1].green,
                            img[r-1][c  ].green,
                            img[r-1][c+1].green,
                            img[r  ][c-1].green,
                            img[r  ][c  ].green,
                            img[r  ][c+1].green,
                            img[r+1][c-1].green,
                            img[r+1][c  ].green,
                            img[r+1][c+1].green);
            const double blue = interpolate(p-pp, 
                            img[r-1][c-1].blue,
                            img[r-1][c  ].blue,
                            img[r-1][c+1].blue,
                            img[r  ][c-1].blue,
                            img[r  ][c  ].blue,
                            img[r  ][c+1].blue,
                            img[r+1][c-1].blue,
                            img[r+1][c  ].blue,
                            img[r+1][c+1].blue);


            rgb_pixel temp;
            assign_pixel(temp.red, red);
            assign_pixel(temp.green, green);
            assign_pixel(temp.blue, blue);
            assign_pixel(result, temp);

            return true;
        }

    private:

        /*  tl tm tr
            ml mm mr
            bl bm br
        */
        // The above is the pixel layout in our little 3x3 neighborhood.  interpolate() will 
        // fit a quadratic to these 9 pixels and then use that quadratic to find the interpolated 
        // value at point p.
        inline double interpolate(
            const dlib::vector<double,2>& p,
            double tl, double tm, double tr, 
            double ml, double mm, double mr, 
            double bl, double bm, double br
        ) const
        {
            matrix<double,6,1> w;
            // x
            w(0) = (tr + mr + br - tl - ml - bl)*0.16666666666;
            // y
            w(1) = (bl + bm + br - tl - tm - tr)*0.16666666666;
            // x^2
            w(2) = (tl + tr + ml + mr + bl + br)*0.16666666666 - (tm + mm + bm)*0.333333333;
            // x*y
            w(3) = (tl - tr - bl + br)*0.25;
            // y^2
            w(4) = (tl + tm + tr + bl + bm + br)*0.16666666666 - (ml + mm + mr)*0.333333333;
            // 1 (constant term)
            w(5) = (tm + ml + mr + bm)*0.222222222 - (tl + tr + bl + br)*0.11111111 + (mm)*0.55555556;

            const double x = p.x();
            const double y = p.y();

            matrix<double,6,1> z;
            z = x, y, x*x, x*y, y*y, 1.0;
                            
            return dot(w,z);
        }
    };

// ----------------------------------------------------------------------------------------
// ----------------------------------------------------------------------------------------
// ----------------------------------------------------------------------------------------

    class black_background
    {
    public:
        template <typename pixel_type>
        void operator() ( pixel_type& p) const { assign_pixel(p, 0); }
    };

    class white_background
    {
    public:
        template <typename pixel_type>
        void operator() ( pixel_type& p) const { assign_pixel(p, 255); }
    };

    class no_background
    {
    public:
        template <typename pixel_type>
        void operator() ( pixel_type& ) const { }
    };

// ----------------------------------------------------------------------------------------

    template <
        typename image_type1,
        typename image_type2,
        typename interpolation_type,
        typename point_mapping_type,
        typename background_type
        >
    void transform_image (
        const image_type1& in_img,
        image_type2& out_img,
        const interpolation_type& interp,
        const point_mapping_type& map_point,
        const background_type& set_background,
        const rectangle& area
    )
    {
        // make sure requires clause is not broken
        DLIB_ASSERT( get_rect(out_img).contains(area) == true &&
                     is_same_object(in_img, out_img) == false ,
            "\t void transform_image()"
            << "\n\t Invalid inputs were given to this function."
            << "\n\t get_rect(out_img).contains(area): " << get_rect(out_img).contains(area)
            << "\n\t get_rect(out_img): " << get_rect(out_img)
            << "\n\t area:              " << area
            << "\n\t is_same_object(in_img, out_img):  " << is_same_object(in_img, out_img)
            );

        const_image_view<image_type1> imgv(in_img);
        image_view<image_type2> out_imgv(out_img);

        for (long r = area.top(); r <= area.bottom(); ++r)
        {
            for (long c = area.left(); c <= area.right(); ++c)
            {
                if (!interp(imgv, map_point(dlib::vector<double,2>(c,r)), out_imgv[r][c]))
                    set_background(out_imgv[r][c]);
            }
        }
    }

// ----------------------------------------------------------------------------------------

    template <
        typename image_type1,
        typename image_type2,
        typename interpolation_type,
        typename point_mapping_type,
        typename background_type
        >
    void transform_image (
        const image_type1& in_img,
        image_type2& out_img,
        const interpolation_type& interp,
        const point_mapping_type& map_point,
        const background_type& set_background
    )
    {
        // make sure requires clause is not broken
        DLIB_ASSERT( is_same_object(in_img, out_img) == false ,
            "\t void transform_image()"
            << "\n\t Invalid inputs were given to this function."
            << "\n\t is_same_object(in_img, out_img):  " << is_same_object(in_img, out_img)
            );

        transform_image(in_img, out_img, interp, map_point, set_background, get_rect(out_img));
    }

// ----------------------------------------------------------------------------------------

    template <
        typename image_type1,
        typename image_type2,
        typename interpolation_type,
        typename point_mapping_type
        >
    void transform_image (
        const image_type1& in_img,
        image_type2& out_img,
        const interpolation_type& interp,
        const point_mapping_type& map_point
    )
    {
        // make sure requires clause is not broken
        DLIB_ASSERT( is_same_object(in_img, out_img) == false ,
            "\t void transform_image()"
            << "\n\t Invalid inputs were given to this function."
            << "\n\t is_same_object(in_img, out_img):  " << is_same_object(in_img, out_img)
            );


        transform_image(in_img, out_img, interp, map_point, black_background(), get_rect(out_img));
    }

// ----------------------------------------------------------------------------------------

    template <
        typename image_type1,
        typename image_type2,
        typename interpolation_type
        >
    point_transform_affine rotate_image (
        const image_type1& in_img,
        image_type2& out_img,
        double angle,
        const interpolation_type& interp
    )
    {
        // make sure requires clause is not broken
        DLIB_ASSERT( is_same_object(in_img, out_img) == false ,
            "\t point_transform_affine rotate_image()"
            << "\n\t Invalid inputs were given to this function."
            << "\n\t is_same_object(in_img, out_img):  " << is_same_object(in_img, out_img)
            );

        const rectangle rimg = get_rect(in_img);


        // figure out bounding box for rotated rectangle
        rectangle rect;
        rect += rotate_point(center(rimg), rimg.tl_corner(), -angle);
        rect += rotate_point(center(rimg), rimg.tr_corner(), -angle);
        rect += rotate_point(center(rimg), rimg.bl_corner(), -angle);
        rect += rotate_point(center(rimg), rimg.br_corner(), -angle);
        set_image_size(out_img, rect.height(), rect.width());

        const matrix<double,2,2> R = rotation_matrix(angle);

        point_transform_affine trans = point_transform_affine(R, -R*dcenter(get_rect(out_img)) + dcenter(rimg));
        transform_image(in_img, out_img, interp, trans);
        return inv(trans);
    }

// ----------------------------------------------------------------------------------------

    template <
        typename image_type1,
        typename image_type2
        >
    point_transform_affine rotate_image (
        const image_type1& in_img,
        image_type2& out_img,
        double angle
    )
    {
        // make sure requires clause is not broken
        DLIB_ASSERT( is_same_object(in_img, out_img) == false ,
            "\t point_transform_affine rotate_image()"
            << "\n\t Invalid inputs were given to this function."
            << "\n\t is_same_object(in_img, out_img):  " << is_same_object(in_img, out_img)
            );

        return rotate_image(in_img, out_img, angle, interpolate_quadratic());
    }

// ----------------------------------------------------------------------------------------

    namespace impl
    {
        class helper_resize_image 
        {
        public:
            helper_resize_image(
                double x_scale_,
                double y_scale_
            ):
                x_scale(x_scale_),
                y_scale(y_scale_)
            {}

            dlib::vector<double,2> operator() (
                const dlib::vector<double,2>& p
            ) const
            {
                return dlib::vector<double,2>(p.x()*x_scale, p.y()*y_scale);
            }

        private:
            const double x_scale;
            const double y_scale;
        };
    }

    template <
        typename image_type1,
        typename image_type2,
        typename interpolation_type
        >
    void resize_image (
        const image_type1& in_img,
        image_type2& out_img,
        const interpolation_type& interp
    )
    {
        // make sure requires clause is not broken
        DLIB_ASSERT( is_same_object(in_img, out_img) == false ,
            "\t void resize_image()"
            << "\n\t Invalid inputs were given to this function."
            << "\n\t is_same_object(in_img, out_img):  " << is_same_object(in_img, out_img)
            );

        const double x_scale = (num_columns(in_img)-1)/(double)std::max<long>((num_columns(out_img)-1),1);
        const double y_scale = (num_rows(in_img)-1)/(double)std::max<long>((num_rows(out_img)-1),1);
        transform_image(in_img, out_img, interp, 
                        dlib::impl::helper_resize_image(x_scale,y_scale));
    }

// ----------------------------------------------------------------------------------------

    template <typename image_type>
    struct is_rgb_image { const static bool value = pixel_traits<typename image_traits<image_type>::pixel_type>::rgb; };
    template <typename image_type>
    struct is_grayscale_image { const static bool value = pixel_traits<typename image_traits<image_type>::pixel_type>::grayscale; };

    // This is an optimized version of resize_image for the case where bilinear
    // interpolation is used.
    template <
        typename image_type1,
        typename image_type2
        >
    typename disable_if_c<(is_rgb_image<image_type1>::value&&is_rgb_image<image_type2>::value) || 
                          (is_grayscale_image<image_type1>::value&&is_grayscale_image<image_type2>::value)>::type 
    resize_image (
        const image_type1& in_img_,
        image_type2& out_img_,
        interpolate_bilinear
    )
    {
        // make sure requires clause is not broken
        DLIB_ASSERT( is_same_object(in_img_, out_img_) == false ,
            "\t void resize_image()"
            << "\n\t Invalid inputs were given to this function."
            << "\n\t is_same_object(in_img_, out_img_):  " << is_same_object(in_img_, out_img_)
            );

        const_image_view<image_type1> in_img(in_img_);
        image_view<image_type2> out_img(out_img_);

        if (out_img.size() == 0 || in_img.size() == 0)
            return;


        typedef typename image_traits<image_type1>::pixel_type T;
        typedef typename image_traits<image_type2>::pixel_type U;
        const double x_scale = (in_img.nc()-1)/(double)std::max<long>((out_img.nc()-1),1);
        const double y_scale = (in_img.nr()-1)/(double)std::max<long>((out_img.nr()-1),1);
        double y = -y_scale;
        for (long r = 0; r < out_img.nr(); ++r)
        {
            y += y_scale;
            const long top    = static_cast<long>(std::floor(y));
            const long bottom = std::min(top+1, in_img.nr()-1);
            const double tb_frac = y - top;
            double x = -x_scale;
            if (pixel_traits<U>::grayscale)
            {
                for (long c = 0; c < out_img.nc(); ++c)
                {
                    x += x_scale;
                    const long left   = static_cast<long>(std::floor(x));
                    const long right  = std::min(left+1, in_img.nc()-1);
                    const double lr_frac = x - left;

                    double tl = 0, tr = 0, bl = 0, br = 0;

                    assign_pixel(tl, in_img[top][left]);
                    assign_pixel(tr, in_img[top][right]);
                    assign_pixel(bl, in_img[bottom][left]);
                    assign_pixel(br, in_img[bottom][right]);

                    double temp = (1-tb_frac)*((1-lr_frac)*tl + lr_frac*tr) + 
                        tb_frac*((1-lr_frac)*bl + lr_frac*br);

                    assign_pixel(out_img[r][c], temp);
                }
            }
            else
            {
                for (long c = 0; c < out_img.nc(); ++c)
                {
                    x += x_scale;
                    const long left   = static_cast<long>(std::floor(x));
                    const long right  = std::min(left+1, in_img.nc()-1);
                    const double lr_frac = x - left;

                    const T tl = in_img[top][left];
                    const T tr = in_img[top][right];
                    const T bl = in_img[bottom][left];
                    const T br = in_img[bottom][right];

                    T temp;
                    assign_pixel(temp, 0);
                    vector_to_pixel(temp, 
                        (1-tb_frac)*((1-lr_frac)*pixel_to_vector<double>(tl) + lr_frac*pixel_to_vector<double>(tr)) + 
                            tb_frac*((1-lr_frac)*pixel_to_vector<double>(bl) + lr_frac*pixel_to_vector<double>(br)));
                    assign_pixel(out_img[r][c], temp);
                }
            }
        }
    }

// ----------------------------------------------------------------------------------------

    template <
        typename image_type1,
        typename image_type2
        >
    struct images_have_same_pixel_types
    {
        typedef typename image_traits<image_type1>::pixel_type ptype1;
        typedef typename image_traits<image_type2>::pixel_type ptype2;
        const static bool value = is_same_type<ptype1, ptype2>::value;
    };

    template <
        typename image_type,
        typename image_type2
        >
    typename enable_if_c<is_grayscale_image<image_type>::value && is_grayscale_image<image_type2>::value && images_have_same_pixel_types<image_type,image_type2>::value>::type 
    resize_image (
        const image_type& in_img_,
        image_type2& out_img_,
        interpolate_bilinear
    )
    {
        // make sure requires clause is not broken
        DLIB_ASSERT( is_same_object(in_img_, out_img_) == false ,
            "\t void resize_image()"
            << "\n\t Invalid inputs were given to this function."
            << "\n\t is_same_object(in_img_, out_img_):  " << is_same_object(in_img_, out_img_)
            );

        const_image_view<image_type> in_img(in_img_);
        image_view<image_type2> out_img(out_img_);

        if (out_img.size() == 0 || in_img.size() == 0)
            return;

        typedef typename image_traits<image_type>::pixel_type T;
        const double x_scale = (in_img.nc()-1)/(double)std::max<long>((out_img.nc()-1),1);
        const double y_scale = (in_img.nr()-1)/(double)std::max<long>((out_img.nr()-1),1);
        double y = -y_scale;
        for (long r = 0; r < out_img.nr(); ++r)
        {
            y += y_scale;
            const long top    = static_cast<long>(std::floor(y));
            const long bottom = std::min(top+1, in_img.nr()-1);
            const double tb_frac = y - top;
            double x = -4*x_scale;

            const simd4f _tb_frac = tb_frac;
            const simd4f _inv_tb_frac = 1-tb_frac;
            const simd4f _x_scale = 4*x_scale;
            simd4f _x(x, x+x_scale, x+2*x_scale, x+3*x_scale);
            long c = 0;
            for (;; c+=4)
            {
                _x += _x_scale;
                simd4i left = simd4i(_x);

                simd4f _lr_frac = _x-left;
                simd4f _inv_lr_frac = 1-_lr_frac; 
                simd4i right = left+1;

                simd4f tlf = _inv_tb_frac*_inv_lr_frac;
                simd4f trf = _inv_tb_frac*_lr_frac;
                simd4f blf = _tb_frac*_inv_lr_frac;
                simd4f brf = _tb_frac*_lr_frac;

                int32 fleft[4];
                int32 fright[4];
                left.store(fleft);
                right.store(fright);

                if (fright[3] >= in_img.nc())
                    break;
                simd4f tl(in_img[top][fleft[0]],     in_img[top][fleft[1]],     in_img[top][fleft[2]],     in_img[top][fleft[3]]);
                simd4f tr(in_img[top][fright[0]],    in_img[top][fright[1]],    in_img[top][fright[2]],    in_img[top][fright[3]]);
                simd4f bl(in_img[bottom][fleft[0]],  in_img[bottom][fleft[1]],  in_img[bottom][fleft[2]],  in_img[bottom][fleft[3]]);
                simd4f br(in_img[bottom][fright[0]], in_img[bottom][fright[1]], in_img[bottom][fright[2]], in_img[bottom][fright[3]]);

                simd4f out = simd4f(tlf*tl + trf*tr + blf*bl + brf*br);
                float fout[4];
                out.store(fout);

                out_img[r][c]   = static_cast<T>(fout[0]);
                out_img[r][c+1] = static_cast<T>(fout[1]);
                out_img[r][c+2] = static_cast<T>(fout[2]);
                out_img[r][c+3] = static_cast<T>(fout[3]);
            }
            x = -x_scale + c*x_scale;
            for (; c < out_img.nc(); ++c)
            {
                x += x_scale;
                const long left   = static_cast<long>(std::floor(x));
                const long right  = std::min(left+1, in_img.nc()-1);
                const float lr_frac = x - left;

                float tl = 0, tr = 0, bl = 0, br = 0;

                assign_pixel(tl, in_img[top][left]);
                assign_pixel(tr, in_img[top][right]);
                assign_pixel(bl, in_img[bottom][left]);
                assign_pixel(br, in_img[bottom][right]);

                float temp = (1-tb_frac)*((1-lr_frac)*tl + lr_frac*tr) + 
                    tb_frac*((1-lr_frac)*bl + lr_frac*br);

                assign_pixel(out_img[r][c], temp);
            }
        }
    }

// ----------------------------------------------------------------------------------------

    template <
        typename image_type
        >
    typename enable_if<is_rgb_image<image_type> >::type resize_image (
        const image_type& in_img_,
        image_type& out_img_,
        interpolate_bilinear
    )
    {
        // make sure requires clause is not broken
        DLIB_ASSERT( is_same_object(in_img_, out_img_) == false ,
            "\t void resize_image()"
            << "\n\t Invalid inputs were given to this function."
            << "\n\t is_same_object(in_img_, out_img_):  " << is_same_object(in_img_, out_img_)
            );

        const_image_view<image_type> in_img(in_img_);
        image_view<image_type> out_img(out_img_);

        if (out_img.size() == 0 || in_img.size() == 0)
            return;


        typedef typename image_traits<image_type>::pixel_type T;
        const double x_scale = (in_img.nc()-1)/(double)std::max<long>((out_img.nc()-1),1);
        const double y_scale = (in_img.nr()-1)/(double)std::max<long>((out_img.nr()-1),1);
        double y = -y_scale;
        for (long r = 0; r < out_img.nr(); ++r)
        {
            y += y_scale;
            const long top    = static_cast<long>(std::floor(y));
            const long bottom = std::min(top+1, in_img.nr()-1);
            const double tb_frac = y - top;
            double x = -4*x_scale;

            const simd4f _tb_frac = tb_frac;
            const simd4f _inv_tb_frac = 1-tb_frac;
            const simd4f _x_scale = 4*x_scale;
            simd4f _x(x, x+x_scale, x+2*x_scale, x+3*x_scale);
            long c = 0;
            for (;; c+=4)
            {
                _x += _x_scale;
                simd4i left = simd4i(_x);
                simd4f lr_frac = _x-left;
                simd4f _inv_lr_frac = 1-lr_frac; 
                simd4i right = left+1;

                simd4f tlf = _inv_tb_frac*_inv_lr_frac;
                simd4f trf = _inv_tb_frac*lr_frac;
                simd4f blf = _tb_frac*_inv_lr_frac;
                simd4f brf = _tb_frac*lr_frac;

                int32 fleft[4];
                int32 fright[4];
                left.store(fleft);
                right.store(fright);

                if (fright[3] >= in_img.nc())
                    break;
                simd4f tl(in_img[top][fleft[0]].red,     in_img[top][fleft[1]].red,     in_img[top][fleft[2]].red,     in_img[top][fleft[3]].red);
                simd4f tr(in_img[top][fright[0]].red,    in_img[top][fright[1]].red,    in_img[top][fright[2]].red,    in_img[top][fright[3]].red);
                simd4f bl(in_img[bottom][fleft[0]].red,  in_img[bottom][fleft[1]].red,  in_img[bottom][fleft[2]].red,  in_img[bottom][fleft[3]].red);
                simd4f br(in_img[bottom][fright[0]].red, in_img[bottom][fright[1]].red, in_img[bottom][fright[2]].red, in_img[bottom][fright[3]].red);

                simd4i out = simd4i(tlf*tl + trf*tr + blf*bl + brf*br);
                int32 fout[4];
                out.store(fout);

                out_img[r][c].red   = static_cast<unsigned char>(fout[0]);
                out_img[r][c+1].red = static_cast<unsigned char>(fout[1]);
                out_img[r][c+2].red = static_cast<unsigned char>(fout[2]);
                out_img[r][c+3].red = static_cast<unsigned char>(fout[3]);


                tl = simd4f(in_img[top][fleft[0]].green,    in_img[top][fleft[1]].green,    in_img[top][fleft[2]].green,    in_img[top][fleft[3]].green);
                tr = simd4f(in_img[top][fright[0]].green,   in_img[top][fright[1]].green,   in_img[top][fright[2]].green,   in_img[top][fright[3]].green);
                bl = simd4f(in_img[bottom][fleft[0]].green, in_img[bottom][fleft[1]].green, in_img[bottom][fleft[2]].green, in_img[bottom][fleft[3]].green);
                br = simd4f(in_img[bottom][fright[0]].green, in_img[bottom][fright[1]].green, in_img[bottom][fright[2]].green, in_img[bottom][fright[3]].green);
                out = simd4i(tlf*tl + trf*tr + blf*bl + brf*br);
                out.store(fout);
                out_img[r][c].green   = static_cast<unsigned char>(fout[0]);
                out_img[r][c+1].green = static_cast<unsigned char>(fout[1]);
                out_img[r][c+2].green = static_cast<unsigned char>(fout[2]);
                out_img[r][c+3].green = static_cast<unsigned char>(fout[3]);


                tl = simd4f(in_img[top][fleft[0]].blue,     in_img[top][fleft[1]].blue,     in_img[top][fleft[2]].blue,     in_img[top][fleft[3]].blue);
                tr = simd4f(in_img[top][fright[0]].blue,    in_img[top][fright[1]].blue,    in_img[top][fright[2]].blue,    in_img[top][fright[3]].blue);
                bl = simd4f(in_img[bottom][fleft[0]].blue,  in_img[bottom][fleft[1]].blue,  in_img[bottom][fleft[2]].blue,  in_img[bottom][fleft[3]].blue);
                br = simd4f(in_img[bottom][fright[0]].blue, in_img[bottom][fright[1]].blue, in_img[bottom][fright[2]].blue, in_img[bottom][fright[3]].blue);
                out = simd4i(tlf*tl + trf*tr + blf*bl + brf*br);
                out.store(fout);
                out_img[r][c].blue   = static_cast<unsigned char>(fout[0]);
                out_img[r][c+1].blue = static_cast<unsigned char>(fout[1]);
                out_img[r][c+2].blue = static_cast<unsigned char>(fout[2]);
                out_img[r][c+3].blue = static_cast<unsigned char>(fout[3]);
            }
            x = -x_scale + c*x_scale;
            for (; c < out_img.nc(); ++c)
            {
                x += x_scale;
                const long left   = static_cast<long>(std::floor(x));
                const long right  = std::min(left+1, in_img.nc()-1);
                const double lr_frac = x - left;

                const T tl = in_img[top][left];
                const T tr = in_img[top][right];
                const T bl = in_img[bottom][left];
                const T br = in_img[bottom][right];

                T temp;
                assign_pixel(temp, 0);
                vector_to_pixel(temp, 
                    (1-tb_frac)*((1-lr_frac)*pixel_to_vector<double>(tl) + lr_frac*pixel_to_vector<double>(tr)) + 
                    tb_frac*((1-lr_frac)*pixel_to_vector<double>(bl) + lr_frac*pixel_to_vector<double>(br)));
                assign_pixel(out_img[r][c], temp);
            }
        }
    }

// ----------------------------------------------------------------------------------------

    template <
        typename image_type1,
        typename image_type2
        >
    void resize_image (
        const image_type1& in_img,
        image_type2& out_img
    )
    {
        // make sure requires clause is not broken
        DLIB_ASSERT( is_same_object(in_img, out_img) == false ,
            "\t void resize_image()"
            << "\n\t Invalid inputs were given to this function."
            << "\n\t is_same_object(in_img, out_img):  " << is_same_object(in_img, out_img)
            );

        resize_image(in_img, out_img, interpolate_bilinear());
    }

// ----------------------------------------------------------------------------------------

    template <
        typename image_type
        >
    void resize_image (
        double size_scale,
        image_type& img 
    )
    {
        // make sure requires clause is not broken
        DLIB_ASSERT( size_scale > 0 ,
            "\t void resize_image()"
            << "\n\t Invalid inputs were given to this function."
            << "\n\t size_scale:  " << size_scale
            );

        image_type temp;
        set_image_size(temp, std::round(size_scale*num_rows(img)), std::round(size_scale*num_columns(img)));
        resize_image(img, temp);
        swap(img, temp);
    }

// ----------------------------------------------------------------------------------------

    template <
        typename image_array_type, 
        typename EXP,
        typename T
        >
    void add_image_rotations (
        const matrix_exp<EXP>& angles,
        image_array_type& images,
        std::vector<std::vector<T> >& objects
    )
    {
        std::vector<std::vector<T> > objects2(objects.size());
        add_image_rotations(angles, images, objects, objects2);
    }

// ----------------------------------------------------------------------------------------
// ----------------------------------------------------------------------------------------

    template <
        typename image_type1,
        typename image_type2,
        typename pyramid_type,
        typename interpolation_type
        >
    void pyramid_up (
        const image_type1& in_img,
        image_type2& out_img,
        const pyramid_type& pyr,
        const interpolation_type& interp
    )
    {
        // make sure requires clause is not broken
        DLIB_ASSERT( is_same_object(in_img, out_img) == false ,
            "\t void pyramid_up()"
            << "\n\t Invalid inputs were given to this function."
            << "\n\t is_same_object(in_img, out_img):  " << is_same_object(in_img, out_img)
            );

        if (image_size(in_img) == 0)
        {
            set_image_size(out_img, 0, 0);
            return;
        }

        rectangle rect = get_rect(in_img);
        rectangle uprect = pyr.rect_up(rect);
        if (uprect.is_empty())
        {
            set_image_size(out_img, 0, 0);
            return;
        }
        set_image_size(out_img, uprect.bottom()+1, uprect.right()+1);

        resize_image(in_img, out_img, interp);
    }

// ----------------------------------------------------------------------------------------

    template <
        typename image_type1,
        typename image_type2,
        typename pyramid_type
        >
    void pyramid_up (
        const image_type1& in_img,
        image_type2& out_img,
        const pyramid_type& pyr
    )
    {
        // make sure requires clause is not broken
        DLIB_ASSERT( is_same_object(in_img, out_img) == false ,
            "\t void pyramid_up()"
            << "\n\t Invalid inputs were given to this function."
            << "\n\t is_same_object(in_img, out_img):  " << is_same_object(in_img, out_img)
            );

        pyramid_up(in_img, out_img, pyr, interpolate_bilinear());
    }

// ----------------------------------------------------------------------------------------

    template <
        typename image_type,
        typename pyramid_type
        >
    void pyramid_up (
        image_type& img,
        const pyramid_type& pyr
    )
    {
        image_type temp;
        pyramid_up(img, temp, pyr);
        swap(temp, img);
    }

// ----------------------------------------------------------------------------------------
// ----------------------------------------------------------------------------------------

    struct chip_dims
    {
        chip_dims (
            unsigned long rows_,
            unsigned long cols_
        ) : rows(rows_), cols(cols_) { }

        unsigned long rows;
        unsigned long cols;
    };


// ----------------------------------------------------------------------------------------

    namespace impl
    {
        template <
            typename image_type1,
            typename image_type2
            >
        void basic_extract_image_chip (
            const image_type1& img,
            const rectangle& location,
            image_type2& chip
        )
        /*!
            ensures
                - This function doesn't do any scaling or rotating. It just pulls out the
                  chip in the given rectangle.  This also means the output image has the
                  same dimensions as the location rectangle.
        !*/
        {
            const_image_view<image_type1> vimg(img);
            image_view<image_type2> vchip(chip);

            vchip.set_size(location.height(), location.width());

            // location might go outside img so clip it
            rectangle area = location.intersect(get_rect(img));

            // find the part of the chip that corresponds to area in img.
            rectangle chip_area = translate_rect(area, -location.tl_corner());

            zero_border_pixels(chip, chip_area);
            // now pull out the contents of area/chip_area.
            for (long r = chip_area.top(), rr = area.top(); r <= chip_area.bottom(); ++r,++rr)
            {
                for (long c = chip_area.left(), cc = area.left(); c <= chip_area.right(); ++c,++cc)
                {
                    assign_pixel(vchip[r][c], vimg[rr][cc]);
                }
            }
        }
    }

// ----------------------------------------------------------------------------------------

}

#endif // DLIB_INTERPOlATIONh_


//  filesystem path.hpp  ---------------------------------------------------------------//

//  Copyright Beman Dawes 2002-2005, 2009
//  Copyright Vladimir Prus 2002

//  Distributed under the Boost Software License, Version 1.0.
//  See http://www.boost.org/LICENSE_1_0.txt

//  Library home page: http://www.boost.org/libs/filesystem

//  path::stem(), extension(), and replace_extension() are based on
//  basename(), extension(), and change_extension() from the original
//  filesystem/convenience.hpp header by Vladimir Prus.

#ifndef BOOST_FILESYSTEM_PATH_HPP
#define BOOST_FILESYSTEM_PATH_HPP

#include <boost/config.hpp>

# if defined( BOOST_NO_STD_WSTRING )
#   error Configuration not supported: Boost.Filesystem V3 and later requires std::wstring support
# endif

#include <boost/filesystem/config.hpp>
#include <boost/filesystem/path_traits.hpp>  // includes <cwchar>
#include <boost/system/error_code.hpp>
#include <boost/system/system_error.hpp>
#include <boost/iterator/iterator_facade.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/io/detail/quoted_manip.hpp>
#include <boost/static_assert.hpp>
#include <boost/functional/hash_fwd.hpp>
#include <boost/type_traits/is_integral.hpp>
#include <boost/type_traits/decay.hpp>
#include <string>
#include <iterator>
#include <cstring>
#include <iosfwd>
#include <stdexcept>
#include <cassert>
#include <locale>
#include <algorithm>

#ifdef BOOST_FILESYSTEM_LOG_DETAIL_APPENDS
# include <iostream>
# define BOOST_FILESYSTEM_APPEND_MSG(X) std::cout << "   " << (X) << "   " << std::endl
#else
# define BOOST_FILESYSTEM_APPEND_MSG(X) ((void)0)
#endif



#include <boost/config/abi_prefix.hpp> // must be the last #include

namespace boost
{
namespace BOOST_FILESYSTEM_NAMESPACE
{
  //------------------------------------------------------------------------------------//
  //                                                                                    //
  //                                    class path                                      //
  //                                                                                    //
  //------------------------------------------------------------------------------------//

  class BOOST_FILESYSTEM_DECL path
  {
  public:

    //  value_type is the character type used by the operating system API to
    //  represent paths.

# ifdef BOOST_WINDOWS_API
    typedef wchar_t                        value_type;
    BOOST_STATIC_CONSTEXPR value_type      preferred_separator = L'\\';
# else 
    typedef char                           value_type;
    BOOST_STATIC_CONSTEXPR value_type      preferred_separator = '/';
# endif
    typedef std::basic_string<value_type>  string_type;  
    typedef std::codecvt<wchar_t, char,
                         std::mbstate_t>   codecvt_type;


    //  ----- character encoding conversions -----

    //  Following the principle of least astonishment, path input arguments
    //  passed to or obtained from the operating system via objects of
    //  class path behave as if they were directly passed to or
    //  obtained from the O/S API, unless conversion is explicitly requested.
    //
    //  POSIX specfies that path strings are passed unchanged to and from the
    //  API. Note that this is different from the POSIX command line utilities,
    //  which convert according to a locale.
    //
    //  Thus for POSIX, char strings do not undergo conversion.  wchar_t strings
    //  are converted to/from char using the path locale or, if a conversion
    //  argument is given, using a conversion object modeled on
    //  std::wstring_convert.
    //
    //  The path locale, which is global to the thread, can be changed by the
    //  imbue() function. It is initialized to an implementation defined locale.
    //  
    //  For Windows, wchar_t strings do not undergo conversion. char strings
    //  are converted using the "ANSI" or "OEM" code pages, as determined by
    //  the AreFileApisANSI() function, or, if a conversion argument is given,
    //  using a conversion object modeled on std::wstring_convert.
    //
    //  See m_pathname comments for further important rationale.

    //  TODO: rules needed for operating systems that use / or .
    //  differently, or format directory paths differently from file paths. 
    //
    //  **********************************************************************************
    //
    //  More work needed: How to handle an operating system that may have
    //  slash characters or dot characters in valid filenames, either because
    //  it doesn't follow the POSIX standard, or because it allows MBCS
    //  filename encodings that may contain slash or dot characters. For
    //  example, ISO/IEC 2022 (JIS) encoding which allows switching to
    //  JIS x0208-1983 encoding. A valid filename in this set of encodings is
    //  0x1B 0x24 0x42 [switch to X0208-1983] 0x24 0x2F [U+304F Kiragana letter KU]
    //                                             ^^^^
    //  Note that 0x2F is the ASCII slash character
    //
    //  **********************************************************************************

    //  Supported source arguments: half-open iterator range, container, c-array,
    //  and single pointer to null terminated string.

    //  All source arguments except pointers to null terminated byte strings support
    //  multi-byte character strings which may have embedded nulls. Embedded null
    //  support is required for some Asian languages on Windows.

    //  [defaults] "const codecvt_type& cvt=codecvt()" default arguments are not used
    //  because some compilers, such as Microsoft prior to VC++ 10, do not handle defaults
    //  correctly in templates.

    //  -----  constructors  -----

    path() BOOST_NOEXCEPT {}                                          
    path(const path& p) : m_pathname(p.m_pathname) {}
//    path(path&& p) BOOST_NOEXCEPT;

#if  BOOST_FILESYSTEM_VERSION == 3 
    //  ---  traditional signatures --

    template <class Source>
    path(Source const& source,
      typename boost::enable_if<path_traits::is_pathable<
        typename boost::decay<Source>::type> >::type* =0)
    {

      path_traits::dispatch(source, m_pathname, codecvt());
    }

    //  Overloads for the operating system API's native character type. Rationale:
    //    - Avoids use of codecvt() for native value_type strings. This limits the
    //      impact of locale("") initialization failures on POSIX systems to programs
    //      that actually depend on locale(""). It further ensures that exceptions thrown
    //      as a result of such failues occur after main() has started, so can be caught.
    //      This is a partial resolution of tickets 4688, 5100, and 5289.
    //    - A slight optimization for a common use case, particularly on POSIX since
    //      value_type is char and that is the most common useage.
    path(const value_type* s) : m_pathname(s) {}
    path(value_type* s) : m_pathname(s) {}
    path(const string_type& s) : m_pathname(s) {}
    path(string_type& s) : m_pathname(s) {}

    template <class Source>
    path(Source const& source, const codecvt_type& cvt)
    //  see [defaults] note above explaining why codecvt() default arguments are not used
    {
      path_traits::dispatch(source, m_pathname, cvt);
    }

    template <class InputIterator>
    path(InputIterator begin, InputIterator end)
    { 
      if (begin != end)
      {
        std::basic_string<typename std::iterator_traits<InputIterator>::value_type>
          s(begin, end);
        path_traits::convert(s.c_str(), s.c_str()+s.size(), m_pathname, codecvt());
      }
    }

    template <class InputIterator>
    path(InputIterator begin, InputIterator end, const codecvt_type& cvt)
    { 
      if (begin != end)
      {
        std::basic_string<typename std::iterator_traits<InputIterator>::value_type>
          s(begin, end);
        path_traits::convert(s.c_str(), s.c_str()+s.size(), m_pathname, cvt);
      }
    }
#else
    //  ---  ISO Technical Specification signatures --

    template <class Source>
    path(const Source& source)
    {
      detail::append(source, m_pathname,
        typename detail::source_tag<Source>::type(),
        typename detail::convert_tag<Source>::type());
    }

    template <class InputIterator>
    path(InputIterator first, InputIterator last)
    {
      detail::append(first, last, m_pathname,
        typename detail::convert_tag<InputIterator>::type());
    }
#endif

    //  -----  assignments  -----

    path& operator=(const path& p)
    {
      m_pathname = p.m_pathname;
      return *this;
    }
//    path& operator=(path&& p) BOOST_NOEXCEPT;

#if  BOOST_FILESYSTEM_VERSION == 3 
    //  ---  traditional signatures --

    template <class Source>
    typename boost::enable_if<path_traits::is_pathable<
      typename boost::decay<Source>::type>, path&>::type
      operator=(Source const& source)
    {
      m_pathname.clear();
      path_traits::dispatch(source, m_pathname, codecvt());
      return *this;
    }

    //  value_type overloads. Same rationale as for constructors above

    path& operator=(const value_type* ptr)  // required in case ptr overlaps *this
    {
      m_pathname = ptr; return *this;
    }
    path& operator=(value_type* ptr)  // required in case ptr overlaps *this
    {
      m_pathname = ptr; return *this;
    }
    path& operator=(const string_type& s) { m_pathname = s; return *this; }
    path& operator=(string_type& s) { m_pathname = s; return *this; }

    path& assign(const value_type* ptr, const codecvt_type&)  // required in case ptr overlaps *this
    {
      m_pathname = ptr; return *this;
    }
    template <class Source>
    path& assign(Source const& source, const codecvt_type& cvt)
    {
      m_pathname.clear();
      path_traits::dispatch(source, m_pathname, cvt);
      return *this;
    }

    template <class InputIterator>
    path& assign(InputIterator begin, InputIterator end)
    {
      return assign(begin, end, codecvt());
    }

    template <class InputIterator>
    path& assign(InputIterator begin, InputIterator end, const codecvt_type& cvt)
    {
      m_pathname.clear();
      if (begin != end)
      {
        std::basic_string<typename std::iterator_traits<InputIterator>::value_type>
          s(begin, end);
        path_traits::convert(s.c_str(), s.c_str()+s.size(), m_pathname, cvt);
      }
      return *this;
    }

#else
    //  ---  ISO Technical Specification signatures --
    template <class Source>
    path& operator=(const Source& source)
    {
      string_type temp;
      detail::append(source, temp,
        typename detail::source_tag<Source>::type(),
        typename detail::convert_tag<Source>::type());
      m_pathname.swap(temp);
      return *this;
    }
    template <class Source>
    path& assign(const Source& source)
    {
      string_type temp;
      detail::append(source, temp,
        typename detail::source_tag<Source>::type(),
        typename detail::convert_tag<Source>::type());
      m_pathname.swap(temp);
        return *this;
    }
    template <class InputIterator>
    path& assign(InputIterator first, InputIterator last)
    {
      string_type temp;
      detail::append(first, last, temp,
        typename detail::convert_tag<InputIterator>::type());
      m_pathname.swap(temp);
      return *this;
    }
#endif

    //  -----  appends  -----

    //  if a separator is added, it is the preferred separator for the platform;
    //  slash for POSIX, backslash for Windows

#if  BOOST_FILESYSTEM_VERSION == 3 
    //  ---  traditional signatures --

    path& operator/=(const path& p);

    template <class Source>
    typename boost::enable_if<path_traits::is_pathable<
      typename boost::decay<Source>::type>, path&>::type
      operator/=(Source const& source)
    {
      return append(source, codecvt());
    }

    path& operator/=(const value_type* ptr);
    path& operator/=(value_type* ptr)
    {
      return this->operator/=(const_cast<const value_type*>(ptr));
    }
    path& operator/=(const string_type& s) { return this->operator/=(path(s)); }
    path& operator/=(string_type& s) { return this->operator/=(path(s)); }

    path& append(const value_type* ptr, const codecvt_type&)  // required in case ptr overlaps *this
    {
      this->operator/=(ptr);
      return *this;
    }

    template <class Source>
    path& append(Source const& source, const codecvt_type& cvt);

    template <class InputIterator>
    path& append(InputIterator begin, InputIterator end)
    {
      return append(begin, end, codecvt());
    }

    template <class InputIterator>
    path& append(InputIterator begin, InputIterator end, const codecvt_type& cvt);

#else
    //  ---  ISO Technical Specification signatures --

    path& operator/=(const path& p);

    template <class Source>
    path& operator/=(const Source& source)
    {
      path temp(source);
      return this->operator/=(temp);
    }
    
    template <class Source>
    path& append(const Source& source)
    {
      path temp(source);
      return this->operator/=(temp);
    }

    template <class InputIterator>
    path& append(InputIterator first, InputIterator last)
    {
      path temp(first, last);
      return this->operator/=(temp);
    }

#endif


    //  -----  concatenation  -----

    path& operator+=(const path& p)         { m_pathname += p.m_pathname; return *this; }

#if  BOOST_FILESYSTEM_VERSION == 3 
    //  ---  traditional signatures --

    template <class Source>
    typename boost::enable_if<path_traits::is_pathable<
      typename boost::decay<Source>::type>, path&>::type
      operator+=(Source const& source)
    {
      return concat(source, codecvt());
    }

    //  value_type overloads. Same rationale as for constructors above
    path& operator+=(const value_type* ptr) { m_pathname += ptr; return *this; }
    path& operator+=(value_type* ptr) { m_pathname += ptr; return *this; }
    path& operator+=(const string_type& s) { m_pathname += s; return *this; }
    path& operator+=(string_type& s) { m_pathname += s; return *this; }
    path& operator+=(value_type c) { m_pathname += c; return *this; }

    template <class CharT>
    typename boost::enable_if<is_integral<CharT>, path&>::type
      operator+=(CharT c)
    {
      CharT tmp[2];
      tmp[0] = c;
      tmp[1] = 0;
      return concat(tmp, codecvt());
    }

    template <class Source>
    path& concat(Source const& source, const codecvt_type& cvt)
    {
      path_traits::dispatch(source, m_pathname, cvt);
      return *this;
    }

    template <class InputIterator>
    path& concat(InputIterator begin, InputIterator end)
    {
      return concat(begin, end, codecvt());
    }

    template <class InputIterator>
    path& concat(InputIterator begin, InputIterator end, const codecvt_type& cvt)
    {
      if (begin == end)
        return *this;
      std::basic_string<typename std::iterator_traits<InputIterator>::value_type>
        s(begin, end);
      path_traits::convert(s.c_str(), s.c_str()+s.size(), m_pathname, cvt);
      return *this;
    }
#else
    //  ---  ISO Technical Specification signatures --

    template <class Source>
    path& operator+=(const Source& source)
    {
      detail::append(source, m_pathname,
        typename detail::source_tag<Source>::type(),
        typename detail::convert_tag<Source>::type());
      return *this;
    }

    path& operator+=(char x)     { return concat(&x, &x+1); }
    path& operator+=(wchar_t x)  { return concat(&x, &x+1); }
# ifdef BOOST_FILESYSTEM_CHAR_16_32_T
    path& operator+=(char16_t x) { return concat(&x, &x+1); }
    path& operator+=(char32_t x) { return concat(&x, &x+1); }
# endif

    template <class Source>
    path& concat(const Source& source)
    {
      detail::append(source, m_pathname,
        typename detail::source_tag<Source>::type(),
        typename detail::convert_tag<Source>::type());
      return *this;
    }

    template <class InputIterator>
    path& concat(InputIterator first, InputIterator last)
    {
      detail::append(first, last, m_pathname,
        typename detail::convert_tag<InputIterator>::type());
      return *this;
    }
#endif


    //  -----  modifiers  -----

    void   clear()             { m_pathname.clear(); }
    path&  make_preferred()
#   ifdef BOOST_POSIX_API
      { return *this; }  // POSIX no effect
#   else // BOOST_WINDOWS_API
      ;  // change slashes to backslashes
#   endif
    path&  remove_filename();
    path&  replace_extension(const path& new_extension = path());
    void   swap(path& rhs)     { m_pathname.swap(rhs.m_pathname); }

    //  -----  observers  -----
  
    //  For operating systems that format file paths differently than directory
    //  paths, return values from observers are formatted as file names unless there
    //  is a trailing separator, in which case returns are formatted as directory
    //  paths. POSIX and Windows make no such distinction.

    //  Implementations are permitted to return const values or const references.

    //  The string or path returned by an observer are specified as being formatted
    //  as "native" or "generic".
    //
    //  For POSIX, these are all the same format; slashes and backslashes are as input and
    //  are not modified.
    //
    //  For Windows,   native:    as input; slashes and backslashes are not modified;
    //                            this is the format of the internally stored string.
    //                 generic:   backslashes are converted to slashes

    //  -----  native format observers  -----

    const string_type&  native() const { return m_pathname; }          // Throws: nothing
    const value_type*   c_str() const  { return m_pathname.c_str(); }  // Throws: nothing

    template <class String>
    String string() const;

    template <class String>
    String string(const codecvt_type& cvt) const;

#   ifdef BOOST_WINDOWS_API
    const std::string string() const { return string(codecvt()); } 
    const std::string string(const codecvt_type& cvt) const
    { 
      std::string tmp;
      if (!m_pathname.empty())
        path_traits::convert(&*m_pathname.begin(), &*m_pathname.begin()+m_pathname.size(),
          tmp, cvt);
      return tmp;
    }
    
    //  string_type is std::wstring, so there is no conversion
    const std::wstring&  wstring() const { return m_pathname; }
    const std::wstring&  wstring(const codecvt_type&) const { return m_pathname; }

#   else   // BOOST_POSIX_API
    //  string_type is std::string, so there is no conversion
    const std::string&  string() const { return m_pathname; }
    const std::string&  string(const codecvt_type&) const { return m_pathname; }

    const std::wstring  wstring() const { return wstring(codecvt()); }
    const std::wstring  wstring(const codecvt_type& cvt) const
    { 
      std::wstring tmp;
      if (!m_pathname.empty())
        path_traits::convert(&*m_pathname.begin(), &*m_pathname.begin()+m_pathname.size(),
          tmp, cvt);
      return tmp;
    }

#   endif

    //  -----  generic format observers  -----

    template <class String>
    String generic_string() const;

    template <class String>
    String generic_string(const codecvt_type& cvt) const;

#   ifdef BOOST_WINDOWS_API
    const std::string   generic_string() const { return generic_string(codecvt()); } 
    const std::string   generic_string(const codecvt_type& cvt) const; 
    const std::wstring  generic_wstring() const;
    const std::wstring  generic_wstring(const codecvt_type&) const { return generic_wstring(); };

#   else // BOOST_POSIX_API
    //  On POSIX-like systems, the generic format is the same as the native format
    const std::string&  generic_string() const  { return m_pathname; }
    const std::string&  generic_string(const codecvt_type&) const  { return m_pathname; }
    const std::wstring  generic_wstring() const { return wstring(codecvt()); }
    const std::wstring  generic_wstring(const codecvt_type& cvt) const { return wstring(cvt); }

#   endif

    //  -----  compare  -----

    int compare(const path& p) const BOOST_NOEXCEPT;  // generic, lexicographical
    int compare(const std::string& s) const { return compare(path(s)); }
    int compare(const value_type* s) const  { return compare(path(s)); }

    //  -----  decomposition  -----

    path  root_path() const; 
    path  root_name() const;         // returns 0 or 1 element path
                                     // even on POSIX, root_name() is non-empty() for network paths
    path  root_directory() const;    // returns 0 or 1 element path
    path  relative_path() const;
    path  parent_path() const;
    path  filename() const;          // returns 0 or 1 element path
    path  stem() const;              // returns 0 or 1 element path
    path  extension() const;         // returns 0 or 1 element path

    //  -----  query  -----

    bool empty() const               { return m_pathname.empty(); } // name consistent with std containers
    bool has_root_path() const       { return has_root_directory() || has_root_name(); }
    bool has_root_name() const       { return !root_name().empty(); }
    bool has_root_directory() const  { return !root_directory().empty(); }
    bool has_relative_path() const   { return !relative_path().empty(); }
    bool has_parent_path() const     { return !parent_path().empty(); }
    bool has_filename() const        { return !m_pathname.empty(); }
    bool has_stem() const            { return !stem().empty(); }
    bool has_extension() const       { return !extension().empty(); }
    bool is_absolute() const
    {
#     ifdef BOOST_WINDOWS_API
      return has_root_name() && has_root_directory();
#     else
      return has_root_directory();
#     endif
    }
    bool is_relative() const         { return !is_absolute(); } 

    //  -----  iterators  -----

    class iterator;
    typedef iterator const_iterator;

    iterator begin() const;
    iterator end() const;

    //  -----  static member functions  -----

    static std::locale  imbue(const std::locale& loc);
    static const        codecvt_type& codecvt();

    //  -----  deprecated functions  -----

# if defined(BOOST_FILESYSTEM_DEPRECATED) && defined(BOOST_FILESYSTEM_NO_DEPRECATED)
#   error both BOOST_FILESYSTEM_DEPRECATED and BOOST_FILESYSTEM_NO_DEPRECATED are defined
# endif

# if !defined(BOOST_FILESYSTEM_NO_DEPRECATED)
    //  recently deprecated functions supplied by default
    path&  normalize()              { return m_normalize(); }
    path&  remove_leaf()            { return remove_filename(); }
    path   leaf() const             { return filename(); }
    path   branch_path() const      { return parent_path(); }
    bool   has_leaf() const         { return !m_pathname.empty(); }
    bool   has_branch_path() const  { return !parent_path().empty(); }
    bool   is_complete() const      { return is_absolute(); }
# endif

# if defined(BOOST_FILESYSTEM_DEPRECATED)
    //  deprecated functions with enough signature or semantic changes that they are
    //  not supplied by default 
    const std::string file_string() const               { return string(); }
    const std::string directory_string() const          { return string(); }
    const std::string native_file_string() const        { return string(); }
    const std::string native_directory_string() const   { return string(); }
    const string_type external_file_string() const      { return native(); }
    const string_type external_directory_string() const { return native(); }

    //  older functions no longer supported
    //typedef bool (*name_check)(const std::string & name);
    //basic_path(const string_type& str, name_check) { operator/=(str); }
    //basic_path(const typename string_type::value_type* s, name_check)
    //  { operator/=(s);}
    //static bool default_name_check_writable() { return false; } 
    //static void default_name_check(name_check) {}
    //static name_check default_name_check() { return 0; }
    //basic_path& canonize();
# endif

//--------------------------------------------------------------------------------------//
//                            class path private members                                //
//--------------------------------------------------------------------------------------//

  private:
#   if defined(_MSC_VER)
#     pragma warning(push) // Save warning settings
#     pragma warning(disable : 4251) // disable warning: class 'std::basic_string<_Elem,_Traits,_Ax>'
#   endif                            // needs to have dll-interface...
/*
      m_pathname has the type, encoding, and format required by the native
      operating system. Thus for POSIX and Windows there is no conversion for
      passing m_pathname.c_str() to the O/S API or when obtaining a path from the
      O/S API. POSIX encoding is unspecified other than for dot and slash
      characters; POSIX just treats paths as a sequence of bytes. Windows
      encoding is UCS-2 or UTF-16 depending on the version.
*/
    string_type  m_pathname;  // Windows: as input; backslashes NOT converted to slashes,
                              // slashes NOT converted to backslashes
#   if defined(_MSC_VER)
#     pragma warning(pop) // restore warning settings.
#   endif 

    string_type::size_type m_append_separator_if_needed();
    //  Returns: If separator is to be appended, m_pathname.size() before append. Otherwise 0.
    //  Note: An append is never performed if size()==0, so a returned 0 is unambiguous.

    void m_erase_redundant_separator(string_type::size_type sep_pos);
    string_type::size_type m_parent_path_end() const;

    path& m_normalize();

    // Was qualified; como433beta8 reports:
    //    warning #427-D: qualified name is not allowed in member declaration 
    friend class iterator;
    friend bool operator<(const path& lhs, const path& rhs);

    // see path::iterator::increment/decrement comment below
    static void m_path_iterator_increment(path::iterator & it);
    static void m_path_iterator_decrement(path::iterator & it);

  };  // class path

  namespace detail
  {
    BOOST_FILESYSTEM_DECL
      int lex_compare(path::iterator first1, path::iterator last1,
        path::iterator first2, path::iterator last2);
    BOOST_FILESYSTEM_DECL
      const path&  dot_path();
    BOOST_FILESYSTEM_DECL
      const path&  dot_dot_path();
  }

# ifndef BOOST_FILESYSTEM_NO_DEPRECATED
  typedef path wpath;
# endif

  //------------------------------------------------------------------------------------//
  //                             class path::iterator                                   //
  //------------------------------------------------------------------------------------//
 
  class path::iterator
    : public boost::iterator_facade<
      path::iterator,
      path const,
      boost::bidirectional_traversal_tag >
  {
  private:
    friend class boost::iterator_core_access;
    friend class boost::filesystem::path;
    friend void m_path_iterator_increment(path::iterator & it);
    friend void m_path_iterator_decrement(path::iterator & it);

    const path& dereference() const { return m_element; }

    bool equal(const iterator & rhs) const
    {
      return m_path_ptr == rhs.m_path_ptr && m_pos == rhs.m_pos;
    }

    // iterator_facade derived classes don't seem to like implementations in
    // separate translation unit dll's, so forward to class path static members
    void increment() { m_path_iterator_increment(*this); }
    void decrement() { m_path_iterator_decrement(*this); }

    path                    m_element;   // current element
    const path*             m_path_ptr;  // path being iterated over
    string_type::size_type  m_pos;       // position of m_element in
                                         // m_path_ptr->m_pathname.
                                         // if m_element is implicit dot, m_pos is the
                                         // position of the last separator in the path.
                                         // end() iterator is indicated by 
                                         // m_pos == m_path_ptr->m_pathname.size()
  }; // path::iterator

  //------------------------------------------------------------------------------------//
  //                                                                                    //
  //                              non-member functions                                  //
  //                                                                                    //
  //------------------------------------------------------------------------------------//

  //  std::lexicographical_compare would infinately recurse because path iterators
  //  yield paths, so provide a path aware version
  inline bool lexicographical_compare(path::iterator first1, path::iterator last1,
    path::iterator first2, path::iterator last2)
    { return detail::lex_compare(first1, last1, first2, last2) < 0; }
  
  inline bool operator==(const path& lhs, const path& rhs)              {return lhs.compare(rhs) == 0;}
  inline bool operator==(const path& lhs, const path::string_type& rhs) {return lhs.compare(rhs) == 0;} 
  inline bool operator==(const path::string_type& lhs, const path& rhs) {return rhs.compare(lhs) == 0;}
  inline bool operator==(const path& lhs, const path::value_type* rhs)  {return lhs.compare(rhs) == 0;}
  inline bool operator==(const path::value_type* lhs, const path& rhs)  {return rhs.compare(lhs) == 0;}
  
  inline bool operator!=(const path& lhs, const path& rhs)              {return lhs.compare(rhs) != 0;}
  inline bool operator!=(const path& lhs, const path::string_type& rhs) {return lhs.compare(rhs) != 0;} 
  inline bool operator!=(const path::string_type& lhs, const path& rhs) {return rhs.compare(lhs) != 0;}
  inline bool operator!=(const path& lhs, const path::value_type* rhs)  {return lhs.compare(rhs) != 0;}
  inline bool operator!=(const path::value_type* lhs, const path& rhs)  {return rhs.compare(lhs) != 0;}

  // TODO: why do == and != have additional overloads, but the others don't?

  inline bool operator<(const path& lhs, const path& rhs)  {return lhs.compare(rhs) < 0;}
  inline bool operator<=(const path& lhs, const path& rhs) {return !(rhs < lhs);}
  inline bool operator> (const path& lhs, const path& rhs) {return rhs < lhs;}
  inline bool operator>=(const path& lhs, const path& rhs) {return !(lhs < rhs);}

  inline std::size_t hash_value(const path& x)
  {
# ifdef BOOST_WINDOWS_API
    std::size_t seed = 0;
    for(const path::value_type* it = x.c_str(); *it; ++it)
      hash_combine(seed, *it == '/' ? L'\\' : *it);
    return seed;
# else   // BOOST_POSIX_API
    return hash_range(x.native().begin(), x.native().end());
# endif
  }

  inline void swap(path& lhs, path& rhs)                   { lhs.swap(rhs); }

  inline path operator/(const path& lhs, const path& rhs)  { return path(lhs) /= rhs; }

  //  inserters and extractors
  //    use boost::io::quoted() to handle spaces in paths
  //    use '&' as escape character to ease use for Windows paths

  template <class Char, class Traits>
  inline std::basic_ostream<Char, Traits>&
  operator<<(std::basic_ostream<Char, Traits>& os, const path& p)
  {
    return os
      << boost::io::quoted(p.template string<std::basic_string<Char> >(), static_cast<Char>('&'));
  }
  
  template <class Char, class Traits>
  inline std::basic_istream<Char, Traits>&
  operator>>(std::basic_istream<Char, Traits>& is, path& p)
  {
    std::basic_string<Char> str;
    is >> boost::io::quoted(str, static_cast<Char>('&'));
    p = str;
    return is;
  }
  
  //  name_checks

  //  These functions are holdovers from version 1. It isn't clear they have much
  //  usefulness, or how to generalize them for later versions.

  BOOST_FILESYSTEM_DECL bool portable_posix_name(const std::string & name);
  BOOST_FILESYSTEM_DECL bool windows_name(const std::string & name);
  BOOST_FILESYSTEM_DECL bool portable_name(const std::string & name);
  BOOST_FILESYSTEM_DECL bool portable_directory_name(const std::string & name);
  BOOST_FILESYSTEM_DECL bool portable_file_name(const std::string & name);
  BOOST_FILESYSTEM_DECL bool native(const std::string & name);
 
//--------------------------------------------------------------------------------------//
//                     class path member template implementation                        //
//--------------------------------------------------------------------------------------//

# if  BOOST_FILESYSTEM_VERSION == 3 

  template <class InputIterator>
  path& path::append(InputIterator begin, InputIterator end, const codecvt_type& cvt)
  { 
    if (begin == end)
      return *this;
    string_type::size_type sep_pos(m_append_separator_if_needed());
    std::basic_string<typename std::iterator_traits<InputIterator>::value_type>
      s(begin, end);
    path_traits::convert(s.c_str(), s.c_str()+s.size(), m_pathname, cvt);
    if (sep_pos)
      m_erase_redundant_separator(sep_pos);
    return *this;
  }

  template <class Source>
  path& path::append(Source const& source, const codecvt_type& cvt)
  {
    if (path_traits::empty(source))
      return *this;
    string_type::size_type sep_pos(m_append_separator_if_needed());
    path_traits::dispatch(source, m_pathname, cvt);
    if (sep_pos)
      m_erase_redundant_separator(sep_pos);
    return *this;
  }
# endif

//--------------------------------------------------------------------------------------//
//                     class path member template specializations                       //
//--------------------------------------------------------------------------------------//

  template <> inline
  std::string path::string<std::string>() const
    { return string(); }

  template <> inline
  std::wstring path::string<std::wstring>() const
    { return wstring(); }

  template <> inline
  std::string path::string<std::string>(const codecvt_type& cvt) const
    { return string(cvt); }

  template <> inline
  std::wstring path::string<std::wstring>(const codecvt_type& cvt) const
    { return wstring(cvt); }

  template <> inline
  std::string path::generic_string<std::string>() const
    { return generic_string(); }

  template <> inline
  std::wstring path::generic_string<std::wstring>() const
    { return generic_wstring(); }

  template <> inline
  std::string path::generic_string<std::string>(const codecvt_type& cvt) const
    { return generic_string(cvt); }

  template <> inline
  std::wstring path::generic_string<std::wstring>(const codecvt_type& cvt) const
    { return generic_wstring(cvt); }



#if  BOOST_FILESYSTEM_VERSION > 3 

  //--------------------------------------------------------------------------------------//
  //                  class path detail::append implementation                            //
  //--------------------------------------------------------------------------------------//

  namespace detail
  {

    //  detail::append overloads, same value_types

    template <class Source> inline
      void append(const Source& from, path::string_type& to,
      iterator_source_tag, no_convert_tag)
    {
       BOOST_FILESYSTEM_APPEND_MSG("append iterator, no conversion" );
      for (auto iter = from; *iter != path::value_type(); ++iter)
        to += *iter;
    }

    template <class Source> inline
      void append(const Source& from, path::string_type& to,
      container_source_tag, no_convert_tag)
    {
       BOOST_FILESYSTEM_APPEND_MSG("append container, no conversion" );
      append(from.cbegin(), from.cend(), to, no_convert_tag());
    }

    template <class InputIterator> inline
      void append(InputIterator first, InputIterator last, path::string_type& to,
      no_convert_tag)
    {
       BOOST_FILESYSTEM_APPEND_MSG("append range, no conversion" );
      to.append(first, last);
    }

    //  append(const directory_entry& dir_entry, ...), is implemented in operations.cpp
    //  where directory_entry is a complete type

    //  detail::append overloads, different value_types so encoding conversion required

    template <class Source> inline
      void append(const Source& from, path::string_type& to,
      iterator_source_tag, with_convert_tag)
    {
       BOOST_FILESYSTEM_APPEND_MSG("append iterator, with conversion" );
      // path_traits::convert() requires a contiguous sequence, so create a temporary string
      std::basic_string<typename source_value_type<typename boost::decay<Source>::type>::type> temp;
      for (auto iter = from;
        *iter != typename source_value_type<typename boost::decay<Source>::type>::type(); ++iter)
        temp += *iter;
      path_traits::convert(temp.c_str(), temp.c_str() + temp.size(), to, path::codecvt());
    }

    template <class Source> inline
      void append(const Source& from, path::string_type& to,
      container_source_tag, with_convert_tag)
    {
       BOOST_FILESYSTEM_APPEND_MSG("append container, with conversion" );
      append(from.cbegin(), from.cend(), to, with_convert_tag());
    }

    template <class InputIterator> inline
      void append(InputIterator first, InputIterator last, path::string_type& to,
      with_convert_tag)
    {
       BOOST_FILESYSTEM_APPEND_MSG("append range, with conversion" );
      // path_traits::convert() requires a contiguous sequence, so create a temporary string
      std::basic_string<typename source_value_type<typename boost::decay<InputIterator>::type>::type>
        temp(first, last);
      path_traits::convert(temp.c_str(), temp.c_str() + temp.size(), to, path::codecvt());
    }

    BOOST_FILESYSTEM_DECL
    void do_append(const directory_entry& dir_entry, path::string_type& to);  // see operations.cpp

    inline
    void append(const directory_entry& dir_entry, path::string_type& to,
      container_source_tag, no_convert_tag)
    {
      BOOST_FILESYSTEM_APPEND_MSG("append directory_entry, no conversion");
      do_append(dir_entry, to);
    }


  }  // namespace detail

#endif

}  // namespace BOOST_FILESYSTEM_NAMESPACE
}  // namespace boost

//--------------------------------------------------------------------------------------//

#include <boost/config/abi_suffix.hpp> // pops abi_prefix.hpp pragmas

#endif  // BOOST_FILESYSTEM_PATH_HPP

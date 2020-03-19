#include <map>
#include <iostream>
#include <cassert>
#include "opencv2/imgcodecs.hpp"
#include "opencv2/highgui.hpp"
#include "opencv2/stitching.hpp"

#include <iostream>
#include <vector>
#include <string>
#include <experimental/filesystem> // http://en.cppreference.com/w/cpp/experimental/fs


using namespace std;
using namespace cv;
namespace fs = std::experimental::filesystem;


vector<fs::path> get_filenames( fs::path path )  {
    vector<fs::path> filenames ;

    // http://en.cppreference.com/w/cpp/experimental/fs/directory_iterator
    const fs::directory_iterator end{} ;

    for( fs::directory_iterator iter{path} ; iter != end ; ++iter )
    {
        // http://en.cppreference.com/w/cpp/experimental/fs/is_regular_file 
        if( fs::is_regular_file(*iter) ) // comment out if all names (names of directories tc.) are required
            filenames.push_back( iter->path() ) ;
    }

    return filenames ;
}




int main(int argc, char **argv) {

  if ( argc != 2 ) {
    printf("usage: process <dir_with_raw_image_files>\n");
    return -1;
  }


  fs::path dir = argv[1];
  fs::path lastfile = "";

  for( const auto& nextfile : get_filenames( dir ) ) {
	if (nextfile.extension() != ".raw") {
		continue;
	}

	if (lastfile == "") {
		lastfile = nextfile;
		continue;
	}
    cout << "processing " << lastfile << " + " << nextfile << '\n' ;
    //pair(double,double) delta = process_two_files(lastfile, nextfile);
  }


  std::map<std::string, Point> m;
  m["hello"] = Point(23,34);
  // check if key is present
  if (m.find("world") != m.end())
    std::cout << "map contains key world!\n";
  // retrieve
  std::cout << m["hello"] << '\n';
  std::map<std::string, Point>::iterator i = m.find("hello");
  assert(i != m.end());
  std::cout << "Key: " << i->first << " Value: " << i->second << '\n';
  return 0;
}

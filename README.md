# OpenRAVE for Ubuntu 22+

## Install Dependencies

### Libraries

Ignore any libraries that are not found.

```bash
sudo apt-get install g++ qt6-base-dev ffmpeg libavcodec-dev libavformat-dev libxvidcore-dev libx264-dev libfaac-dev libogg-dev libvorbis-dev libdc1394-dev libgsm1-dev libboost-dev libboost-regex-dev libxml2-dev libglew-dev  libboost-graph-dev libboost-wave-dev libboost-serialization-dev libboost-filesystem-dev libpcre3-dev libboost-thread-dev libmpfr-dev libboost-date-time-dev libqhull-dev libswscale-dev libfcl-dev libbullet-dev libboost-iostreams-dev rapidjson-dev libgpgmepp-dev python3-pyqt6 liblapack-dev python3-opengl libopenscenegraph-dev mesa-common-dev
```

## Install OpenRAVE

```bash
make
```

```bash
sudo make install
```

Add Python3 bindings to python path:

```bash
export PYTHONPATH=$PYTHONPATH:`openrave-config --python-dir`
```

Optionally, add this line to `~/.bashrc` or run:

```bash
cd build
source openrave.bash
```

which sets the python path and others.

## Examples

Most Python examples are out of date and will not run. This one will at least run for a while until it fails. Note that it ignores collisions for some reason.

```bash
openrave.py --example simplemanipulation
```

## Tests

To run the python test cases:

```bash
sudo apt install python3-nose
```

```bash
python3 test/run_tests.py
```


<!-- Welcome to OpenRAVE
-------------------

`Official OpenRAVE Homepage <http://openrave.org>`_



------

Continuous integration for this project is generously made possible by:

.. image:: teamcity.jpg
  :target: https://www.jetbrains.com/teamcity/ -->

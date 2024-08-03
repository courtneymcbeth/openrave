# OpenRAVE for Ubuntu 22

## Install Dependencies

### Qt4 PPA

```bash
sudo add-apt-repository ppa:ubuntuhandbook1/ppa
```

### Libraries

Ignore any libraries that are not found.

```bash
sudo apt-get install g++ libqt4-dev qt4-dev-tools ffmpeg libavcodec-dev libavformat-dev libxvidcore-dev libx264-dev libfaac-dev libogg-dev libvorbis-dev libdc1394-dev liblame-dev libgsm1-dev libboost-dev libboost-regex-dev libxml2-dev libglew-dev  libboost-graph-dev libboost-wave-dev libboost-serialization-dev libboost-filesystem-dev libpcre3-dev libboost-thread-dev libmpfr-dev libboost-date-time-dev libqhull-dev libswscale-dev lapack3-dev
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

Optionally, add this line to `~/.bashrc`.

## Examples

Most Python examples are out of date and will not run. This one will at least run for a while until it fails. Note that it ignores collisions for some reason.

```bash
openrave.py --example simplemanipulation
```


<!-- Welcome to OpenRAVE
-------------------

`Official OpenRAVE Homepage <http://openrave.org>`_



------

Continuous integration for this project is generously made possible by:

.. image:: teamcity.jpg
  :target: https://www.jetbrains.com/teamcity/ -->

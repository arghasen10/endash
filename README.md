# EnDash-5G

`EnDash-5G`: A wrapper over the popular DASH-based ABR streaming algorithm, which utilizes network aware video chunk download. Entire implementation is done on ns-3.

### Installation
To add EnDash in ns3 apply the following steps.

```bash
cd ns3-mmwave
./src/create-module.py endash
rm -rf endash
git clone https://github.com/arghasen10/endash.git
cd endash
```

### Run ns3 script
To start the simulation apply the following steps.
```bash
cd ns3-mmwave
./waf configure
./waf build
cp src/endash/examples/endash/multicell-endash.cc scratch/
./waf --run scratch/multicell-endash
```
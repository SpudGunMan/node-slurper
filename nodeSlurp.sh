#!/bin/bash
# node-slurper.sh
# 2024 Kelly Keeton K7MHI - MIT License
# Tool uses meshtastic CLI methods to slurp node data from a meshtastic device

cwd=$(pwd)
echo "                              "
echo "    o   o        o            "
echo "    |\  |        |            "
echo "    | \ | o-o  o-O o-o        "
echo "    |  \| | | |  | |-'        "
echo "    o   o o-o  o-o o-o        "
echo "                              "
echo " o-o  o                       "
echo "|     |                       "
echo " o-o  | o  o o-o o-o  o-o o-o "
echo "    | | |  | |   |  | |-' |   "
echo "o--o  o o--o o   O-o  o-o o   "
echo "                 |            "
echo "                 o            "
echo "                              "
echo " slurping node...             "
echo "                              "

# look for meshtastic has a bin in the path
if whereis meshtastic | grep -q bin/meshtastic; then
    # collect node-info.txt
    meshtastic --info > $cwd/node-info.txt
    # collect node.yaml
    meshtastic --export-config > $cwd/node.yaml

    # evaluate the node-info.txt for meshtastic version
    if grep -q "pip install --upgrade meshtastic" $cwd/node-info.txt; then
        echo "meshtastic tools need to be updated run one of the following commands"
        echo "pip install --upgrade meshtastic"
        echo "pip install --upgrade meshtastic --break-system-packages"
    fi
    # exit check
    if grep -q "No Serial Meshtastic device detected" $cwd/node-info.txt; then
        echo "No Meshtastic device found exiting"
        exit 1
    fi
else
    # install meshtastic if it is not found
    echo "meshtastic not found"
    sudo apt-get install -y python3-pip
    pip3 install -U meshtastic
fi

# parse the node-info.txt for node number
nodeNum=$(grep -o '"myNodeNum": [0-9]*' $cwd/node-info.txt | grep -o '[0-9]*')

#move the node-info.txt to nodeNum-Info.txt
mv $cwd/node-info.txt $cwd/$nodeNum-Info.txt

# channel keys string
channelKeys=$(grep -o '"psk": ".*", "name": ".*"' $cwd/$nodeNum-Info.txt | grep -o '"psk": ".*"')

#move the node.yaml to nodeNum.yaml
mv $cwd/node.yaml $cwd/$nodeNum.yaml

# parse the owner from the node.yaml
owner=$(grep -o 'owner: ".*"' $cwd/$nodeNum.yaml | grep -o '".*"')

# MQTT password 
mqttPass=$(grep -o 'password: .*' $cwd/$nodeNum.yaml | grep -o ' .*')

# bluetooth pin "fixedPin: 123456" the pin is 123456
fixedPin=$(grep -o 'fixedPin: [0-9]*' $cwd/$nodeNum.yaml | grep -o '[0-9]*')

# wifi password
wifiPsk=$(grep -o 'wifiPsk: .*' $cwd/$nodeNum.yaml | grep -o ' .*')

# admin key string
adminKey=$(grep -o ' \-.*' $cwd/$nodeNum.yaml | grep -o ' .*')

# dm private key
dmPrivateKey=$(grep -o 'privateKey: .*' $cwd/$nodeNum.yaml | grep -o ' .*')

# dm public key
dmPublicKey=$(grep -o 'publicKey: .*' $cwd/$nodeNum.yaml | grep -o ' .*')

echo "node data slurped for node $nodeNum named $owner"
echo "channel keys:" 
echo "$channelKeys"
echo "fixed pin:$fixedPin"
echo "wifi password:$wifiPsk"
echo "mqtt password:$mqttPass"
echo "admin key:$adminKey"
echo "dm private key:$dmPrivateKey"
echo "dm public key:$dmPublicKey"
echo "node data saved to $nodeNum-Info.txt and $nodeNum.yaml"
echo ""

# ask if user wants to inject any keys into the node
echo "Do you want to inject any data into the node?"
read -p "y/n: " -n 1 -r
if [[ ! $REPLY =~ ^[Yy]$ ]]; then
    echo ""
    echo "exiting, happy meshing!"
    exit 1
fi

echo "attempting to inject commands into the node, please wait..."
#meshtastic --ch-add MeshAround --ch-set psk base64:
#meshtastic --set security.admin_key base64:
#meshtastic --ch-add admin --ch-set psk base64:
echo "no commands to inject"
#meshtastic --set lora.region US
echo "done, happy meshing!"
exit 0
# end of script

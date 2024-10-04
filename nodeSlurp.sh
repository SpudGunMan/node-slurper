#!/bin/bash
# node-slurper.sh
# 2024 Kelly Keeton K7MHI - MIT License
# Tool uses meshtastic CLI methods to slurp node data from a meshtastic device

# User defined variables
injectKeys=true

function injectData() {
    # inject commands into the node update the following commands with your data
    # uncomment and add your keys to the end of line
    echo ""
    echo "attempting to inject commands into the node, please wait..."
    #meshtastic --ch-add MeshAround --ch-set psk base64:
    #sleep 1
    #meshtastic --set security.admin_key base64:
    #sleep 1
    #meshtastic --ch-add admin --ch-set psk base64:
    #sleep 1
    #meshtastic --set lora.region US
    #echo "done, happy meshing!"
    echo "no commands to inject"
    echo ""
    echo "waiting for new device or press ctrl-c to exit"
}

# end of user defined variables

cwd=$(pwd)

function slurpData() {
    # no device detected
    if grep -q "No Serial Meshtastic device detected" $cwd/node-info.txt; then
        echo "plug a device in via USB"
    else
        # parse the node-info.txt for node number
        nodeNum=$(grep -o '"myNodeNum": [0-9]*' $cwd/node-info.txt| grep -o '[0-9]*')

        #move the node-info.txt to nodeNum-Info.txt
        mv $cwd/node-info.txt $cwd/$nodeNum-Info.txt

        #move the node.yaml to nodeNum.yaml
        mv $cwd/node.yaml $cwd/$nodeNum.yaml

        # channel keys string
        channelKeys=$(grep -o '"psk": ".*", "name": ".*"' $cwd/$nodeNum-Info.txt | grep -o '"psk": ".*"')

        # parse the owner from the node.yaml
        owner=$(grep -o 'owner: ".*"' $cwd/$nodeNum.yaml | grep -o '".*"')

        # MQTT password 
        mqttPass=$(grep -o 'password: .*' $cwd/$nodeNum.yaml | grep -o ' .*')

        # bluetooth pin
        fixedPin=$(grep -o 'fixedPin: [0-9]*' $cwd/$nodeNum.yaml | grep -o '[0-9]*')

        # wifi password
        wifiPsk=$(grep -o 'wifiPsk: .*' $cwd/$nodeNum.yaml | grep -o ' .*')

        # admin key string
        adminKey=$(grep -o ' \-.*' $cwd/$nodeNum.yaml | grep -o ' .*')

        # dm private key
        dmPrivateKey=$(grep -o 'privateKey: .*' $cwd/$nodeNum.yaml | grep -o ' .*')

        # dm public key
        dmPublicKey=$(grep -o 'publicKey: .*' $cwd/$nodeNum.yaml | grep -o ' .*')

        # display the important node data to stdout
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
        if [ "$injectKeys" = false ]; then
            echo "waiting for new device or press ctrl-c to exit"
        fi
    fi
}

function saveData() {
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

    # no device detected
    if grep -q "No Serial Meshtastic device detected" $cwd/node-info.txt; then
        echo "No device detected"
    else
        # parse the node-info.txt for node number
        nodeNum=$(grep -o '"myNodeNum": [0-9]*' $cwd/node-info.txt | grep -o '[0-9]*')
        echo "node $nodeNum detected"
    fi
}

# start of main script

echo "                                "
echo "    o   o        o              "
echo "    |\  |        |              "
echo "    | \ | o-o  o-O o-o          "
echo "    |  \| | | |  | |-'          "
echo "    o   o o-o  o-o o-o          "
echo "                                "
echo " o-o  o                         "
echo "|     |                         "
echo " o-o  | o  o o-o o-o  o-o o-o   "
echo "    | | |  | |   |  | |-' |     "
echo "o--o  o o--o o   O-o  o-o o     "
echo "                 |              "
echo "                 o              "
echo "                                "
echo " waiting for node to connect    "
echo "                                "
echo "                    spudgunman24"
echo "                                "

# look for meshtastic has a bin in the path
if whereis meshtastic | grep -q bin/meshtastic; then

    # Assume a device is connected already
    echo " trying to slurp, could take a moment..."
    # collect node-info.txt and node.yaml
    saveData

    # slurp the data
    slurpData

    if [ "$injectKeys" = true ]; then
        injectData
    fi

    # detect new device loop for any subsequent devices
    while true; do
        
        # detect new USB serial device
        ls /dev/tty* > $cwd/.nodeSlurp.before
        sleep 2
        ls /dev/tty* > $cwd/.nodeSlurp.after

        if diff $cwd/.nodeSlurp.before $cwd/.nodeSlurp.after; then
            sleep 1
        else
            echo "new device detected trying to slurp, could take a moment..."
            echo ""

            # collect node-info.txt and node.yaml
            saveData

            # slurp the data
            slurpData

            if [ "$injectKeys" = true ]; then
                injectData
            fi
        fi
    done
else
    # if it is not found
    echo "meshtastic not found"
    echo "sudo apt-get install -y python3-pip"
    echo "pip install -U meshtastic"
    echo "(or) pip install -U meshtastic --break-system-packages"
    exit 1
fi

exit 0
# end of script

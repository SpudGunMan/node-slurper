# node-slurper.ps1
# 2024 Kelly Keeton K7MHI - MIT License
# Tool uses meshtastic CLI methods to slurp node data from a meshtastic device

# User defined variables
$injectKeys = $true

function Inject-Data {
    Write-Output ""
    Write-Output "attempting to inject commands into the node, please wait..."
    # Uncomment and add your keys to the end of line
    # meshtastic --ch-add MeshAround --ch-set psk base64:
    # Start-Sleep -Seconds 1
    # meshtastic --set security.admin_key base64:
    # Start-Sleep -Seconds 1
    # meshtastic --ch-add admin --ch-set psk base64:
    # Start-Sleep -Seconds 1
    # meshtastic --set lora.region US
    meshtastic --ch-set module_settings.position_precision 32 --ch-index 0
    # Write-Output "done, happy meshing!"
    Write-Output "no commands to inject"
    Write-Output ""
    Write-Output "waiting for new device or press ctrl-c to exit"
}

$cwd = Get-Location

function Slurp-Data {
    if (Select-String -Path "$cwd\node-info.txt" -Pattern "No Serial Meshtastic device detected") {
        Write-Output "plug a device in via USB"
    } else {
        $nodeNum = (Select-String -Path "$cwd\node-info.txt" -Pattern '"myNodeNum": [0-9]*' | ForEach-Object { $_.Matches[0].Value -match '[0-9]+'; $matches[0] })
        Move-Item -Path "$cwd\node-info.txt" -Destination "$cwd\$nodeNum-Info.txt"
        Move-Item -Path "$cwd\node.yaml" -Destination "$cwd\$nodeNum.yaml"

        $channelKeys = (Select-String -Path "$cwd\$nodeNum-Info.txt" -Pattern '"psk": ".*", "name": ".*"' | ForEach-Object { $_.Matches[0].Value -match '"psk": ".*"'; $matches[0] })
        $owner = (Select-String -Path "$cwd\$nodeNum.yaml" -Pattern 'owner: ".*"' | ForEach-Object { $_.Matches[0].Value -match '".*"'; $matches[0] })
        $mqttPass = (Select-String -Path "$cwd\$nodeNum.yaml" -Pattern 'password: .*' | ForEach-Object { $_.Matches[0].Value -match ' .*'; $matches[0] })
        $fixedPin = (Select-String -Path "$cwd\$nodeNum.yaml" -Pattern 'fixedPin: [0-9]*' | ForEach-Object { $_.Matches[0].Value -match '[0-9]+'; $matches[0] })
        $wifiPsk = (Select-String -Path "$cwd\$nodeNum.yaml" -Pattern 'wifiPsk: .*' | ForEach-Object { $_.Matches[0].Value -match ' .*'; $matches[0] })
        $adminKey = (Select-String -Path "$cwd\$nodeNum.yaml" -Pattern ' \-.*' | ForEach-Object { $_.Matches[0].Value -match ' .*'; $matches[0] })
        $dmPrivateKey = (Select-String -Path "$cwd\$nodeNum.yaml" -Pattern 'privateKey: .*' | ForEach-Object { $_.Matches[0].Value -match ' .*'; $matches[0] })
        $dmPublicKey = (Select-String -Path "$cwd\$nodeNum.yaml" -Pattern 'publicKey: .*' | ForEach-Object { $_.Matches[0].Value -match ' .*'; $matches[0] })

        Write-Output "node data slurped for node $nodeNum named $owner"
        Write-Output "channel keys:"
        Write-Output "$channelKeys"
        Write-Output "fixed pin:$fixedPin"
        Write-Output "wifi password:$wifiPsk"
        Write-Output "mqtt password:$mqttPass"
        Write-Output "admin key:$adminKey"
        Write-Output "dm private key:$dmPrivateKey"
        Write-Output "dm public key:$dmPublicKey"
        Write-Output "node data saved to $nodeNum-Info.txt and $nodeNum.yaml"
        Write-Output ""
        if (-not $injectKeys) {
            Write-Output "waiting for new device or press ctrl-c to exit"
        }
    }
}

function Save-Data {
    meshtastic --info > "$cwd\node-info.txt"
    meshtastic --export-config > "$cwd\node.yaml"

    if (Select-String -Path "$cwd\node-info.txt" -Pattern "pip install --upgrade meshtastic") {
        Write-Output "meshtastic tools need to be updated run one of the following commands"
        Write-Output "pip install --upgrade meshtastic"
        Write-Output "pip install --upgrade meshtastic --break-system-packages"
    }

    if (Select-String -Path "$cwd\node-info.txt" -Pattern "No Serial Meshtastic device detected") {
        Write-Output "No device detected"
    } else {
        $nodeNum = (Select-String -Path "$cwd\node-info.txt" -Pattern '"myNodeNum": [0-9]*' | ForEach-Object { $_.Matches[0].Value -match '[0-9]+'; $matches[0] })
        Write-Output "node $nodeNum detected"
    }
}

# Start of main script

Write-Output "                                "
Write-Output "    o   o        o              "
Write-Output "    |\  |        |              "
Write-Output "    | \ | o-o  o-O o-o          "
Write-Output "    |  \| | | |  | |-'          "
Write-Output "    o   o o-o  o-o o-o          "
Write-Output "                                "
Write-Output " o-o  o                         "
Write-Output "|     |                         "
Write-Output " o-o  | o  o o-o o-o  o-o o-o   "
Write-Output "    | | |  | |   |  | |-' |     "
Write-Output "o--o  o o--o o   O-o  o-o o     "
Write-Output "                 |              "
Write-Output "                 o              "
Write-Output "                                "
Write-Output " waiting for node to connect    "
Write-Output "                                "
Write-Output "                    spudgunman24"
Write-Output "                                "

if (Get-Command meshtastic -ErrorAction SilentlyContinue) {
    Write-Output " trying to slurp, could take a moment..."
    Save-Data
    Slurp-Data

    if ($injectKeys) {
        Inject-Data
    }

    while ($true) {
        $before = Get-ChildItem /dev/tty*
        Start-Sleep -Seconds 2
        $after = Get-ChildItem /dev/tty*

        if ($before -ne $after) {
            Write-Output "new device detected trying to slurp, could take a moment..."
            Write-Output ""
            Save-Data
            Slurp-Data

            if ($injectKeys) {
                Inject-Data
            }
        } else {
            Start-Sleep -Seconds 1
        }
    }
} else {
    Write-Output "meshtastic not found"
    Write-Output "pip3 install --upgrade meshtastic"
    exit 1
}

exit 0
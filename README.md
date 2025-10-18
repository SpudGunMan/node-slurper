# node-slurper
Meshtastic bulk node management tools, run from linux command line

## Usage

```sh
$ ./nodeSlurp.sh
```

```
    o   o        o            
    |\  |        |            
    | \ | o-o  o-O o-o        
    |  \| | | |  | |-'        
    o   o o-o  o-o o-o        
                              
 o-o  o                       
|     |                       
 o-o  | o  o o-o o-o  o-o o-o 
    | | |  | |   |  | |-' |   
o--o  o o--o o   O-o  o-o o   
                 |            
                 o            
                              
 slurping node...           
                              
node data slurped for node 425675309 named "DEMO"
channel keys: 
"psk": "ABC123=", "name": "Banter", "channelNum": 0, "id": 0, "uplinkEnabled": false, "downlinkEnabled"
"psk": "ABC123=", "name": "Chat", "channelNum": 0, "id": 0, "uplinkEnabled": false, "downlinkEnabled"
fixed pin: 123456
wifi password: password
mqtt password: lpassword
admin key: ABC123=
dm private key: ABC123=
dm public key: ABC123=
node data saved to 425675309-Info.txt and 425675309.yaml
```

---

## Other Tools

### mudpSlurp

A cli to slup the UDP data from mesh nodes

### qconfig.sh

A graphical quick-configurator for Meshtastic nodes using YAD (Yet Another Dialog).  not maintained.  
**Usage:**
```sh
$ ./qconfig.sh
```

---

## Requirements

- Bash YAD (for qconfig.sh)
- Python and stuff



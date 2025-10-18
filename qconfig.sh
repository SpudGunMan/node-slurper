# Store YAD form output in variable for processing
FORM_DATA=$(yad --form \
    --title="Meshtastic Quick Configurator" \
    --text="Configure your Meshtastic device" \
    --columns=2 \
    --field="Region":CB "US!EU" \
    --field="Roll":CB "Client!Client_Mute" \
    --field="LORA Profile":CB "LongFast!LongSlow!VLongFast!VLongSlow!ShortFast!ShortSlow!MediumFast!MediumSlow" \
    --field="Short Name":RW \
    --field="Long Name":RW \
    --field="LORA Frequency Override":RW \
    --field="MQTT":CHK \
    --field="MQTT Server":RW \
    --field="MQTT User":RW \
    --field="MQTT Password":RW \
    --field="BlueTooth":CHK \
    --field="Bluetooth Pin":RW \
    --field="Channel":CB "0!1!2!3!4!5!6!7" \
    --field="Channel Security":CB \
    --field="Channel Name":RW \
    --field="Channel Key":RW \
    --field="Channel OK-MQTT":CHK \
    --field="Admin Public Key":RW \
    --field="Position Precision":CB "32!21!16" \
    --button="Save":1 \
    --button="Exit":3 \
    --on-change=6 "sed -i '3s/.*/'\"\$6\"'/' %3")

# Handle the Save button
if [ $? -eq 1 ]; then
    echo "Form data: $FORM_DATA"
    # Process form data here
fi
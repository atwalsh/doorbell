from flask import Flask, request
from PIL import Image
from decouple import config
from slack_sdk import WebClient
from slack_sdk.errors import SlackApiError
import tempfile

app = Flask(__name__)

client = WebClient(token=config('SLACK_BOT_TOKEN'))


@app.route('/pic', methods=['POST'])
def pic():
    with tempfile.NamedTemporaryFile() as nf:
        f = request.files['imageFile']
        f.save(nf.name)

        # Rotate the image since the ESP32-CAM is upside down
        with Image.open(nf.name) as im:
            im.rotate(180).save(nf.name, format='JPEG')

        # Send the file to slack
        try:
            response = client.files_upload(channels=config('SLACK_CHANNEL'), file=nf.name)
        except SlackApiError as e:
            print(f"Got an error: {e.response['error']}")

    return '', 200


if __name__ == '__main__':
    app.run(debug=config('DEBUG', default=False, cast=bool))

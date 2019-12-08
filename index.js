const server = require('server');
const { get, post } = server.router;
const axios = require('axios');

const auth = "xxxxxxxxxxxxxxxxxxxxxxxxxxx";

function blynk(pin) {
    //return `http://blynk-cloud.com/${auth}/update/${pin}`;
    return `http://188.166.206.43/${auth}/update/${pin}`;
}

server({ port: process.env.PORT }, [
    get('/on', async ctx => {
        try {
            await axios.put(blynk("D2"), [ "1" ]);
        } catch(err) {
            return err.toString();
        }
        return "OK!";
    }),
    get('/off', ctx => {
        axios.put(blynk("D2"), [ "0" ]);
        return "OK!";
    }),
]);

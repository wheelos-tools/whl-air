// signaling_server/config.js
const path = require('path');

module.exports = {
    server: {
        port: process.env.PORT || 8898, // Listen on port 8898, or environment variable PORT
        ssl: {
            enabled: process.env.SSL_ENABLED === 'true', // Enable SSL based on env var
            key: process.env.SSL_KEY_PATH || path.join(__dirname, 'certs', 'server.key'),
            cert: process.env.SSL_CERT_PATH || path.join(__dirname, 'certs', 'server.crt')
        }
    },
    auth: {
        jwtSecret: process.env.JWT_SECRET || 'your_default_jwt_secret', // Use strong secret from env var!
        tokenExpiration: '1h' // Example token expiration
    },
    // Add other configurations, like CORS headers if necessary
};

# signaling_server/Dockerfile
# Use a lightweight Node.js image
FROM node:18-alpine

# Set the working directory in the container
WORKDIR /app

# Copy package.json and package-lock.json (if any)
COPY package*.json ./

# Install dependencies
RUN npm install

# Copy the rest of the application code
COPY src ./src
COPY config.js ./
COPY certs ./certs # Copy the certs directory

# Expose the port the server listens on (adjust if you change the default port)
EXPOSE 8898

# Command to run the application
CMD ["npm", "start"]

# Optional: Add a non-root user for security
# RUN adduser -D appuser
# USER appuser

#include <iostream>
#include <string>
#include <memory>

typedef std::string RawResponse;
struct curl{
    int response_code;
    std::string error;
};

class IResponse{
    public:
        virtual std::string getBody() = 0;
        virtual int getResponseCode() = 0;
        virtual std::string getError() = 0;
};

class CurlResponse: public IResponse {
    private:
        std::string body_;
        int response_code_;
        std::string error_;
    public:
        CurlResponse(std::string body, curl c): body_(body) {
            this->response_code_ = c.response_code ;
            this->error_ = c.error;
        }

        std::string getBody() override {
            return this->body_;
        }    

        int getResponseCode() override {
            return this->response_code_;
        }

        std::string getError() override {
            return this->error_;
        }
};
// void pushquote(std::string);
// void pushint(int);

// class I4glResult{
//     public:
//     int get_state_id();
//     int return_result();
// };

// class StatusResult{
//     private:
//         std::string skip_ {"TURE"};
//         std::string unique_key_;
//         int state_id_;

//     public:

//     int get_stat_id(){
//         return this->state_id_;
//     }
//     int return_result(){
//         pushquote(this->skip_);
//         pushquote(this->unique_key_);
//         pushint(this->state_id_);
//     }
// };

class IResponseHandler {
// Chain of responsibility for handling responses
// https://en.wikipedia.org/wiki/Chain-of-responsibility_pattern
    protected:
    IResponseHandler* nextHandler = NULL;
    public:
    virtual void handle(CurlResponse) = 0;
    virtual IResponseHandler& addHandler(IResponseHandler* handler){
        this->nextHandler = handler;
        // Avoid dereferencing null pointer
        return handler ? *handler: *this;
    };
};

class BadResponseHandler: public IResponseHandler {
    // Handler for HTTP error codes.
    // If the response contains any http code considered
    // unsuccessfull/failure, throw a BadResponseError.
    // Previous links in the chain can catch the BadResponseError
    // and perform appropriate actions with the response object.
    public:
    
    class BadResponseError: public std::exception{
        std::string message;
        public:
        BadResponseError(std::string message):message(message){};
        virtual const char* what() const throw(){
            return "Bad HTTP Response";
        }; 
    };
    void handle(CurlResponse response) {
        int response_code = response.getResponseCode();
        if(response_code < 200 || response_code > 299 ){
            // request failed
            std::cout << "Handling bad http response" << std::endl;
            throw BadResponseError(std::to_string(response_code));
        }
        if(this->nextHandler)
            this->nextHandler->handle(response);
    }
};

class GuardianAlertHandler: public IResponseHandler {
    // Handles sending guardian alerts for any kind of
    // BadResponseError received in the chain.
    // GuardianAlertHandler will catch a BadResponseError,
    // perform necessary actions for guardian,
    // Then throw the execption again for other links to handle.
    
    public:
    void handle(CurlResponse response) {
        try{
            if(this->nextHandler)
                this->nextHandler->handle(response);
        } catch(BadResponseHandler::BadResponseError& e) {
            std::cout<< "Caught bad response, alerting!" << std::endl;
            std::cout<< "Alert!" << std::endl;
             throw e;
        }
    }
};

class StatusHandler: public IResponseHandler {
    // Intended to be a root handler for all compliance status requests.
    // StatusHandler will try to parse the response based on http code
    // and then content in the body.
    // If http is a failure, StatusHandler will pass the response on up the chain
    // and listen for a BadResponseError.
    
    public:
    void handle(CurlResponse response) {
        try {
            auto response_code = response.getResponseCode();
            if(response_code >= 200 && response_code <= 299){
                std::cout 
                << "Successfull http response!" 
                << std::endl;
                
                std::cout 
                << "Parsing Response body: "
                << response.getBody() 
                << std::endl;

                std::cout 
                << "Sending record to be dialed..."
                << std::endl;
            }else{
                if(this->nextHandler)
                    this->nextHandler->handle(response);
            }
        } catch(BadResponseHandler::BadResponseError e) {
            std::cout << "Bad http response, skipping..." << std::endl;
        }
    }
};

class UpdateHandler: public IResponseHandler {

public:
    void handle(CurlResponse response) {
        try {
            if(this->nextHandler)
                this->nextHandler->handle(response);
            auto response_code = response.getResponseCode();
            if(response_code >= 200 && response_code <= 299){
                std::cout << "Successfull http update! " << std::endl;
            }
        } catch (BadResponseHandler::BadResponseError& e) {

            std::cout 
            << "Bad response on update request: " 
            << e.what()
            << std::endl;

            std::cout
            << "Nothing to do, just log it"
            << std::endl;
        }
    }
};


int main() {

    RawResponse resp = "Hello world HTML!";
    curl c;
    c.response_code = 200;
    c.error = "";

    CurlResponse response(resp, c);

    StatusHandler statushandler;
    GuardianAlertHandler guardianHandler;
    BadResponseHandler badResponseHandler;
    statushandler.addHandler(&guardianHandler).addHandler(&badResponseHandler);

    try{
        statushandler.handle(response);
    }catch(std::exception e){
        std::cout << "Caught http exception: " << e.what() << std::endl;
    }

    c.response_code = 404;

    CurlResponse updateResponse(resp, c);

    UpdateHandler updateHandler;
    updateHandler.addHandler(&guardianHandler).addHandler(&badResponseHandler);

    updateHandler.handle(updateResponse);

}
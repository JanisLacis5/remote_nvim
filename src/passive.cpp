// main for the pasive app that is called on the client 
// main() {
    // is started when ssh -R has been opened by the remote
    // open `ssh -L 7778:127.0.0.1:7777 SERVER_USER@SERVER`. in the future, the port could be in the received packet
    // start listening / reding /tmp/janisnvim.sock
    // run `nvim --server /tmp/janisnvim.sock --remote-ui`
    // connect(127.0.0.1:7778)
    // read nvim events from /tmp/janisnvim.sock, process them and forward to the remote headless nvim server
    // receive events from nvim server and push them to the local nvim remote ui
// }

int main() {}

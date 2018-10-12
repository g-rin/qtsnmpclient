# Contributing

I like pull requests from everyone. By participating in this project, you agree to abide by the thoughtbot code of conduct.

Fork, then clone the repo:

    git clone git@github.com:your-username/qtsnmpclient.git

Go to the downloaded project:

    cd qtsnmpclient/

Build to project inside "build" directory:

    mkdir build
    cd build
    qmake -r CONFIG+=static ../projects/all.pro
    make -j8

Run the test:

    ./release/bin/manual_test 

Make your change. Add tests for your change. And check it by your tests.

Push to your fork and submit a pull request.

At this point you're waiting on us. I like to at least comment on pull requests within three business days (and, typically, one business day). We may suggest some changes or improvements or alternatives.

Some things that will increase the chance that your pull request is accepted:

    Write tests.
    Follow our style guide.
    Write a good commit message.

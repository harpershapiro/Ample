#include "MainComponent.h"
#include "Sequencer.h"

#include <vector>
#include <algorithm>

MainComponent::MainComponent()
	: state_(PlayState::Stopped),
	sequencer_(8, 120.0)
{
    // specify the number of input and output channels that we want to open
    setAudioChannels (0, 2);

	addAndMakeVisible(&open_button_kick_);
	open_button_kick_.setButtonText("Open Kick...");
	// this data should be passed to the sampler_source but HOW!?
	open_button_kick_.onClick = [this] { open_button_kick_clicked(); };
	
	addAndMakeVisible(&open_button_snare_);
	open_button_snare_.setButtonText("Open Snare...");
	// this data should be passed to the sampler_source but HOW!?
	open_button_snare_.onClick = [this] { open_button_snare_clicked(); };
	
	/* Define sample assignment buttons. */
	for (auto& button : sample_assigners_)
	{
		button = std::make_unique<SequencerButton>();
		addAndMakeVisible(button.get());
		button->setColour(TextButton::buttonColourId, Colours::greenyellow);
		button->is_on_ = false;

		button->onClick = [&button] {
			button->is_on_ = !button->is_on_;
			button->toggle_on_off_colour();
		};
	}


	addAndMakeVisible(&play_button_);
	play_button_.setButtonText("Play");
	play_button_.onClick = [this] { play_button_clicked(); };
	play_button_.setColour(TextButton::buttonColourId, Colours::green);
	play_button_.setEnabled(false);

	addAndMakeVisible(&stop_button_);
	stop_button_.setButtonText("Stop");
	stop_button_.onClick = [this] { stop_button_clicked(); };
	stop_button_.setColour(TextButton::buttonColourId, Colours::red);
	stop_button_.setEnabled(false);
	
	sampler_source_kick_.addChangeListener(this);
	sampler_source_snare_.addChangeListener(this);
	sequencer_.addChangeListener(this);

	// Make sure you set the size of the component after
	// you add any child components.
	setSize(800, 600);
}

MainComponent::~MainComponent()
{
    shutdownAudio();
	sampler_source_kick_.releaseResources();
	sampler_source_snare_.releaseResources();
	sequencer_.stop();
}

void MainComponent::prepareToPlay (int samplesPerBlockExpected, double sampleRate)
{
	sampler_source_kick_.prepareToPlay(samplesPerBlockExpected, sampleRate);
	sampler_source_snare_.prepareToPlay(samplesPerBlockExpected, sampleRate);
	mixer_source_.prepareToPlay(samplesPerBlockExpected, sampleRate);

	mixer_source_.addInputSource(&sampler_source_kick_, false);
	mixer_source_.addInputSource(&sampler_source_snare_, false);
}

void MainComponent::getNextAudioBlock (const AudioSourceChannelInfo& bufferToFill)
{
	if (sampler_source_kick_.is_empty() || sampler_source_snare_.is_empty())
	{
		bufferToFill.clearActiveBufferRegion();
		return;
	}

	mixer_source_.getNextAudioBlock(bufferToFill);
}

void MainComponent::releaseResources()
{
	mixer_source_.releaseResources();
	sampler_source_kick_.releaseResources();
	sampler_source_snare_.releaseResources();
}

void MainComponent::paint (Graphics& g)
{
    // (Our component is opaque, so we must completely fill the background with a solid colour)
    g.fillAll (getLookAndFeel().findColour (ResizableWindow::backgroundColourId));

    // You can add your drawing code here!
}

void MainComponent::resized()
{
    // This is called when the MainContentComponent is resized.
    // If you add any child components, this is where you should
    // update their positions.
	open_button_kick_.setBounds(10, 10, getWidth() - 20, 20);
	open_button_snare_.setBounds(10, 40, getWidth() - 20, 20);

	play_button_.setBounds(10, 70, getWidth() - 20, 20);
	stop_button_.setBounds(10, 100, getWidth() - 20, 20);

	int i = 200;
	for (auto& button : sample_assigners_)
	{
		button->setBounds(i, 200, 40, 40);
		i += 50;
	}
}

void MainComponent::changeListenerCallback(ChangeBroadcaster * source)
{
	if (source == &sampler_source_kick_)
	{
		if (sampler_source_kick_.is_playing())
			change_state(PlayState::Playing);
		else
			change_state(PlayState::Stopped);
	}

	else if (source == &sequencer_)
	{
		bool even_step = sequencer_.current_step() % 2 == 0;
		if (sequencer_.play_at_current_trigger_ && !even_step)
		{ 
			sampler_source_kick_.start();
		}
		else if (sequencer_.play_at_current_trigger_ && even_step)
		{
			sampler_source_snare_.start();
		}
		else
		{
			sampler_source_kick_.stop();
			sampler_source_snare_.stop();
		}

		/* Handle button drawing here... */
		trigger_button_color(sequencer_.current_step());
	}
}

void MainComponent::change_state(PlayState new_state)
{
	if (state_ != new_state)
	{
		state_ = new_state;
		switch (state_)
		{
		case PlayState::Stopped:                          
			stop_button_.setEnabled(false);
			play_button_.setEnabled(true);
			sampler_source_kick_.set_position(0.0);
			sampler_source_kick_.set_playing(false);
			break;
		case PlayState::Starting:                          
			play_button_.setEnabled(false);
			stop_button_.setEnabled(true);
			break;
		case PlayState::Playing:                           
			stop_button_.setEnabled(true);
			break;
		case PlayState::Stopping:                          
			play_button_.setEnabled(true);
			sampler_source_kick_.stop();
			break;
		}
	}
}

void MainComponent::play_button_clicked()
{
	change_state(PlayState::Starting);
}

void MainComponent::stop_button_clicked()
{
	change_state(PlayState::Stopping);
}

void MainComponent::open_button_kick_clicked()
{
	FileChooser chooser("Select a WAV file to play... ",
		File::nonexistent,
		"*.wav");

	if (chooser.browseForFileToOpen())
	{
		auto file = chooser.getResult();
		auto path = file.getFullPathName();
		sampler_source_kick_.set_file_path(path);
		sampler_source_kick_.notify();
	}

	// This should not be stopping but for now it will do.
	change_state(PlayState::Stopping);
	sampler_source_kick_.set_playing(false);
}

void MainComponent::open_button_snare_clicked()
{
	FileChooser chooser("Select a WAV file to play... ",
		File::nonexistent,
		"*.wav");

	if (chooser.browseForFileToOpen())
	{
		auto file = chooser.getResult();
		auto path = file.getFullPathName();
		sampler_source_snare_.set_file_path(path);
		sampler_source_snare_.notify();
	}

	// This should not be stopping but for now it will do.
	change_state(PlayState::Stopping);
	sampler_source_snare_.set_playing(false);
}

void MainComponent::trigger_button_color(uint16_t step_to_update)
{
	// if we're not updating the first step we can just set the previous step to the old color.
	if (step_to_update != 0)
		sample_assigners_.at(step_to_update - 1)->toggle_on_off_colour();
	else
		sample_assigners_.at(sample_assigners_.size() - 1)->toggle_on_off_colour();
		
	sample_assigners_.at(step_to_update)->trigger_sequencer_colour();
}
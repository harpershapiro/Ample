/*
  ==============================================================================

    SampleSource.cpp
    Created: 19 Sep 2018 4:58:00pm
    Author:  bennet

  ==============================================================================
*/

#include "SampleSource.h"

SampleSource::SampleSource()
	: Thread("")
{
	format_manager_.registerBasicFormats();
	startThread();
}

SampleSource::~SampleSource() 
{
	current_buffer_ = nullptr;
	stopThread(500);
}

void SampleSource::getNextAudioBlock(const AudioSourceChannelInfo& buffer_to_fill)
{
	if (!is_playing())
	{
		buffer_to_fill.clearActiveBufferRegion();
		return;
	}

	// Once we know we're actually playing audio we can proceed with loading and playing data.
	RefCountedBuffer::Ptr retained_current_buffer(current_buffer_);

	if (retained_current_buffer == nullptr)
	{
		buffer_to_fill.clearActiveBufferRegion();
		return;
	}

	auto cur_audio_sampler_buffer = retained_current_buffer->get_audio_sample_buffer();
	int32_t position = retained_current_buffer->position_;

	int num_in_channels = cur_audio_sampler_buffer->getNumChannels();
	int num_out_channels = buffer_to_fill.buffer->getNumChannels();

	int out_samples_remaining = buffer_to_fill.numSamples;
	int out_samples_offset = 0;

	while (out_samples_remaining > 0)
	{
		int buffer_samples_remaining = cur_audio_sampler_buffer->getNumSamples() - position_;
		int samples_this_iter = jmin(out_samples_remaining, buffer_samples_remaining);

		for (int channel = 0; channel < num_out_channels; ++channel)
		{
			buffer_to_fill.buffer->copyFrom(channel,
				buffer_to_fill.startSample + out_samples_offset,
				*(cur_audio_sampler_buffer),
				channel % num_in_channels,
				position_,
				samples_this_iter);
		}
	
		out_samples_remaining -= samples_this_iter;
		out_samples_offset += samples_this_iter;
		set_position(position_ + samples_this_iter); 

		if (position_ == cur_audio_sampler_buffer->getNumSamples())
		{
			set_position(0);
			is_playing_ = false;
			buffer_to_fill.clearActiveBufferRegion();
			return;
		}
	}

	retained_current_buffer->position_ = position;
}

void SampleSource::run()
{
	while (!threadShouldExit())
	{
		check_for_path_to_open();
		free_stale_buffers();
		wait(500);
	}
}

void SampleSource::start()
{
	if (!is_playing_ && !is_empty())
	{
		/* 
		 * Once is_playing_ is set to true, the getNextAudioBlock method should
		 * be able to start playing audio. If is_playing_ is false, it will return
		 * and clear the active audio buffer.
		 */
		is_playing_ = true;
		sendChangeMessage();
	}
}

void SampleSource::stop()
{
	if (is_playing_)
	{
		is_playing_ = false;
		position_ = 0.0;
		sendChangeMessage();
	}
}

void SampleSource::free_stale_buffers()
{
	for (auto i = buffers_.size(); --i >= 0;)                           // [1]
	{
		RefCountedBuffer::Ptr buffer(buffers_.getUnchecked(i));			// [2]
		if (buffer->getReferenceCount() == 2)							// [3]
			buffers_.remove(i);
	}
}

void SampleSource::check_for_path_to_open()
{
	String path_to_open;
	path_to_open.swapWith(chosen_path_);
	if (path_to_open.isNotEmpty())
	{
		File file(path_to_open);
		std::unique_ptr<AudioFormatReader> reader(format_manager_.createReaderFor(file));
		if (reader.get() != nullptr)
		{
			auto duration = reader->lengthInSamples / reader->sampleRate;
			if (duration < 2)
			{
				RefCountedBuffer::Ptr new_buffer = new RefCountedBuffer(file.getFileName(), reader->numChannels, (int)reader->lengthInSamples);
				reader->read(new_buffer->get_audio_sample_buffer().get(), 0, (int)reader->lengthInSamples, 0, true, true);
				current_buffer_ = new_buffer;
				buffers_.add(new_buffer);
			}
			else
			{
				// handle the error that the file is 2 seconds or longer..
			}
		}
	}
}

std::optional<std::shared_ptr<AudioSampleBuffer>> SampleSource::get_buffer()
{
	if (current_buffer_)
		return std::make_optional<std::shared_ptr<AudioSampleBuffer>>(current_buffer_->get_audio_sample_buffer());
	else
		return std::nullopt;
}

bool SampleSource::is_empty()
{
	auto buffer = get_buffer();
	if (!buffer.has_value())
		return true;

	return ((*get_buffer())->getNumChannels() == 0) && ((*get_buffer())->getNumSamples() == 0);
}

void SampleSource::set_size(int num_channels, int num_samples)
{
	auto buffer = get_buffer();
	if (buffer.has_value())
	{
		(*buffer)->setSize(num_channels, num_samples);
	}
}

void SampleSource::set_position(double pos)
{
	position_ = pos;
}

void SampleSource::prepareToPlay(int samples_per_block_expected, double sample_rate)
{
	/* Do nothing? */
}

void SampleSource::releaseResources()
{
	/* Do nothing? */
}